/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MergeImagesReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "MergeImagesDialog.h"

#include <QFileInfo>
#include <QSet>

#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>

#include <algorithm>
#include <sstream>

namespace tomviz {

MergeImagesReaction::MergeImagesReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  updateEnableState();
}

void MergeImagesReaction::onTriggered()
{
  // Post a dialog box to select what kind of merging to do, either merging
  // arrays or merging components in a single array.
  MergeImagesDialog dialog;

  // TODO - build list widget of arrays and their inputs in the dialog,
  // allow for reordering

  int result = dialog.exec();
  if (result == QDialog::Rejected) {
    return;
  }

  DataSource* newSource = nullptr;
  if (dialog.getMode() == MergeImagesDialog::Arrays) {
    newSource = mergeArrays();
  } else {
    newSource = mergeComponents();
  }

  if (newSource) {
    LoadDataReaction::dataSourceAdded(newSource);
  }
}

void MergeImagesReaction::updateDataSources(QSet<DataSource*> sources)
{
  m_dataSources = sources;
  updateEnableState();
}

void MergeImagesReaction::updateEnableState()
{
  bool enabled = m_dataSources.size() > 1;

  // Check for compatibility of the DataSource extents. Ignore overlap in
  // physical space for now.
  if (enabled) {
    QList<DataSource*> sourceList = m_dataSources.values();
    auto info = sourceList[0]->proxy()->GetDataInformation();
    int refExtent[6];
    info->GetExtent(refExtent);

    // Check against other DataSource extents
    for (int i = 0; i < m_dataSources.size(); ++i) {
      info = sourceList[i]->proxy()->GetDataInformation();
      int thisExtent[6];
      info->GetExtent(thisExtent);
      enabled = enabled && std::equal(refExtent, refExtent + 6, thisExtent);
    }
  }

  parentAction()->setEnabled(enabled);
}

DataSource* MergeImagesReaction::mergeArrays()
{
  if (m_dataSources.size() < 1) {
    return nullptr;
  }

  QList<DataSource*> sourceList = m_dataSources.values();

  // Eventually, we'll offer the option to merge components in a single array.
  // For now, we will simply append the point data arrays.
  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  vtkSMSourceProxy* filter = vtkSMSourceProxy::SafeDownCast(
    pxm->NewProxy("filters", "AppendAttributes"));
  Q_ASSERT(filter);

  for (int i = 0; i < sourceList.size(); ++i) {
    vtkSMPropertyHelper(filter, "Input").Add(sourceList[i]->proxy(), 0);
  }

  filter->UpdateVTKObjects();
  filter->UpdatePipeline();

  DataSource* newSource = new DataSource(filter);
  QString mergedFilename(QFileInfo(sourceList[0]->fileName()).baseName());
  for (int i = 1; i < sourceList.size(); ++i) {
    mergedFilename.append(" + ");
    mergedFilename.append(QFileInfo(sourceList[i]->fileName()).baseName());
  }
  newSource->setFileName(mergedFilename);
  filter->Delete();

  return newSource;
}

DataSource* MergeImagesReaction::mergeComponents()
{
  if (m_dataSources.size() < 1) {
    return nullptr;
  }

  QList<DataSource*> sourceList = m_dataSources.values();
  QStringList sourceLabels;

  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  vtkSMSourceProxy* filter = vtkSMSourceProxy::SafeDownCast(
    pxm->NewProxy("filters", "PythonCalculator"));
  Q_ASSERT(filter);

  std::stringstream expression;
  expression << "np.transpose(np.vstack((";

  for (int i = 0; i < sourceList.size(); ++i) {
    vtkSMPropertyHelper(filter, "Input").Add(sourceList[i]->proxy(), 0);

    auto info = sourceList[0]->proxy()->GetDataInformation();
    auto pointData = info->GetPointDataInformation();
    for (int j = 0; j < pointData->GetNumberOfArrays(); ++j) {
      auto arrayInfo = pointData->GetArrayInformation(j);
      std::string arrayName(arrayInfo->GetName());
      expression << "inputs[" << i << "].PointData['" << arrayName << "']";
      if (i < sourceList.size() - 1 || j < pointData->GetNumberOfArrays() - 1) {
        expression << ", ";
      }
    }

    sourceLabels.append(sourceList[i]->label());
  }

  // Arrange columns
  expression << ")))";

  std::cout << "expression: " << expression.str() << std::endl;

  // Set the Python expression for arranging the components in the output
  vtkSMPropertyHelper(filter, "ArrayAssociation").Set(0);
  vtkSMPropertyHelper(filter, "CopyArrays").Set(0);
  vtkSMPropertyHelper(filter, "Expression").Set(expression.str().c_str());
  vtkSMPropertyHelper(filter, "ArrayName").Set("Merged");

  filter->UpdateVTKObjects();
  filter->UpdatePipeline();

  DataSource* newSource = new DataSource(filter);
  newSource->setFileName("Merged Image");
  filter->Delete();

  // Give the components names based off the labels of the data sources
  // that were used to generate them.
  newSource->setComponentNames(sourceLabels);
  return newSource;
}

} // namespace tomviz
