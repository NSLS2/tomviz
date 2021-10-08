/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveLoadStateReaction.h"

#include "ModuleManager.h"
#include "RecentFilesMenu.h"
#include "Tvh5Format.h"
#include "Utilities.h"

#include <pqSettings.h>
#include <vtkSMProxyManager.h>

#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>

#include <vtk_pugixml.h>

namespace tomviz {

SaveLoadStateReaction::SaveLoadStateReaction(QAction* parentObject, bool load)
  : pqReaction(parentObject), m_load(load)
{}

void SaveLoadStateReaction::onTriggered()
{
  if (m_load) {
    loadState();
  } else {
    saveState();
  }
}

bool SaveLoadStateReaction::saveState()
{
  QString tvh5Filter = "Tomviz full state files (*.tvh5)";
  QString tvsmFilter = "Tomvis state files (*.tvsm)";
  QStringList filters;
  filters << tvh5Filter
          << tvsmFilter
          << "All files (*)";

  QFileDialog fileDialog(tomviz::mainWidget(), tr("Save State File"),
                         QString(), filters.join(";;"));
  fileDialog.setObjectName("SaveStateDialog");
  fileDialog.setAcceptMode(QFileDialog::AcceptSave);
  fileDialog.setFileMode(QFileDialog::AnyFile);
  if (fileDialog.exec() == QDialog::Accepted) {
    QString filename = fileDialog.selectedFiles()[0];
    QString format = fileDialog.selectedNameFilter();
    if (format == tvh5Filter && !filename.endsWith(".tvh5")) {
      filename = QString("%1%2").arg(filename, ".tvh5");
    } else if (format == tvsmFilter && !filename.endsWith(".tvsm")) {
      filename = QString("%1%2").arg(filename, ".tvsm");
    }
    bool success = SaveLoadStateReaction::saveState(filename);
    if (success) {
      // Only set the most recent state file if the user picked a file
      // to save via a file dialog, and the save was successful.
      ModuleManager::instance().setMostRecentStateFile(filename);
    }
    return success;
  }
  return false;
}

bool SaveLoadStateReaction::loadState()
{
  QStringList filters;
  filters << "Tomvis state files (*.tvsm *.tvh5)"
          << "All files (*)";

  QFileDialog fileDialog(tomviz::mainWidget(), tr("Load State File"),
                         QString(), filters.join(";;"));
  fileDialog.setObjectName("LoadStateDialog");
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  if (fileDialog.exec() == QDialog::Accepted) {
    return SaveLoadStateReaction::loadState(fileDialog.selectedFiles()[0]);
  }
  return false;
}

bool SaveLoadStateReaction::loadState(const QString& filename)
{
  if (ModuleManager::instance().hasDataSources()) {
    if (QMessageBox::Yes !=
        QMessageBox::warning(tomviz::mainWidget(), "Load State Warning",
                             "Current data and operators will be cleared when "
                             "loading a state file.  Proceed anyway?",
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)) {
      return false;
    }
  }

  bool success = false;
  if (filename.endsWith(".tvh5")) {
    success = loadTvh5(filename);
  } else if (filename.endsWith(".tvsm")) {
    success = loadTvsm(filename);
  } else {
    qCritical() << "Unknown state format for file: " << filename;
    return false;
  }

  if (success) {
    RecentFilesMenu::pushStateFile(filename);
    // Set the most recent state file if we successfully loaded a
    // state, whether it was done programmatically or via file dialog
    ModuleManager::instance().setMostRecentStateFile(filename);
  }

  return success;
}

bool SaveLoadStateReaction::loadTvh5(const QString& filename)
{
  return Tvh5Format::read(filename.toStdString());
}

bool SaveLoadStateReaction::loadTvsm(const QString& filename)
{
  QFile openFile(filename);
  if (!openFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open state file.");
    return false;
  }

  QJsonParseError error;
  auto contents = openFile.readAll();
  auto doc = QJsonDocument::fromJson(contents, &error);
  bool legacyStateFile = false;
  if (doc.isNull()) {
    // See if user is trying to load a old XML base state file.
    if (error.error == QJsonParseError::IllegalValue) {
      legacyStateFile = checkForLegacyStateFileFormat(contents);
    }

    // If its a legacy state file we are done.
    if (legacyStateFile) {
      return false;
    }
  }

  auto executeOnLoad = automaticallyExecutePipelines();
  ModuleManager::instance().executePipelinesOnLoad(executeOnLoad);

  if (doc.isObject()) {
    // This needs to run here, but needs to run after the dialog is connected
    // and execed.  Otherwise we miss signals fired from within deserialize.
    // So put it on a timer.
    QTimer::singleShot(0, [doc, filename]() {
      ModuleManager::instance().deserialize(doc.object(),
                                            QFileInfo(filename).dir());
    });
    QDialog dialog(tomviz::mainWidget(), Qt::WindowStaysOnTopHint);
    QHBoxLayout* layout = new QHBoxLayout();
    QLabel* label = new QLabel("Please wait... loading state file");
    layout->addWidget(label);
    dialog.setLayout(layout);
    connect(&ModuleManager::instance(), &ModuleManager::stateDoneLoading,
            &dialog, &QDialog::accept);
    dialog.exec();
    if (ModuleManager::instance().lastLoadStateSucceeded()) {
      return true;
    } // else continue to error message
  }

  if (!legacyStateFile) {
    QMessageBox::warning(
      tomviz::mainWidget(), "Invalid state file",
      QString("Unable to read state file: %1").arg(error.errorString()));
  }

  return false;
}

bool SaveLoadStateReaction::saveState(const QString& fileName, bool interactive)
{
  if (fileName.endsWith(".tvsm")) {
    return saveTvsm(fileName, interactive);
  } else if (fileName.endsWith(".tvh5")) {
    return saveTvh5(fileName);
  }

  qCritical() << "Unknown format for saveState(): " << fileName;
  return false;
}

bool SaveLoadStateReaction::saveTvsm(const QString& fileName, bool interactive)
{
  QFileInfo info(fileName);
  QFile saveFile(fileName);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
    return false;
  }

  QJsonObject state;
  auto success =
    ModuleManager::instance().serialize(state, info.dir(), interactive);
  QJsonDocument doc(state);
  auto writeSuccess = saveFile.write(doc.toJson());
  return success && writeSuccess != -1;
}

bool SaveLoadStateReaction::saveTvh5(const QString& fileName)
{
  return Tvh5Format::write(fileName.toStdString());
}

QString SaveLoadStateReaction::extractLegacyStateFileVersion(
  const QByteArray state)
{
  QString fullVersion;
  pugi::xml_document doc;

  if (doc.load_buffer(state.data(), state.size())) {
    pugi::xpath_node version = doc.select_node("/tomvizState/version");

    if (version) {
      fullVersion = version.node().attribute("full").value();
    }
  }

  return fullVersion;
}

bool SaveLoadStateReaction::automaticallyExecutePipelines()
{
  QSettings* settings = pqApplicationCore::instance()->settings();
  QString key = "PipelineSettings.AutoExecuteOnStateLoad";
  if (settings->contains(key)) {
    return settings->value(key).toBool();
  }

  QDialog dialog(tomviz::mainWidget());
  dialog.setFixedWidth(300);
  dialog.setMaximumHeight(50);
  dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QVBoxLayout vLayout;
  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("Load state"));

  QFormLayout formLayout;
  vLayout.addLayout(&formLayout);

  QLabel title(tr("Automatically execute pipelines?"));
  formLayout.addRow(&title);

  QCheckBox dontAskAgain("Don't ask again");
  formLayout.addRow(&dontAskAgain);

  QDialogButtonBox buttons(QDialogButtonBox::Yes | QDialogButtonBox::No);
  vLayout.addWidget(&buttons);

  connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto r = dialog.exec();

  bool executePipelines = r == QDialog::Accepted;
  if (dontAskAgain.isChecked()) {
    settings->setValue(key, executePipelines);
  }

  return executePipelines;
}

bool SaveLoadStateReaction::checkForLegacyStateFileFormat(
  const QByteArray state)
{

  auto version = extractLegacyStateFileVersion(state);
  if (version.length() > 0) {
    QString url = "https://github.com/OpenChemistry/tomviz/releases";
    QString versionString = QString("Tomviz %1").arg(version);
    if (!version.contains("-g")) {
      url = QString("https://github.com/OpenChemistry/tomviz/releases/%1")
              .arg(version);
      versionString = QString("<a href=%1>Tomviz %2</a>").arg(url).arg(version);
    }

    QMessageBox versionWarning(tomviz::mainWidget());
    versionWarning.setIcon(QMessageBox::Icon::Warning);
    versionWarning.setTextFormat(Qt::RichText);
    versionWarning.setWindowTitle("Trying to load a legacy state file?");
    versionWarning.setText(
      QString("This state file was written using %1."
              " The format is not supported by the version of Tomviz you are "
              "running. "
              "A compatible version can be downloaded <a href=%2>here<a>")
        .arg(versionString)
        .arg(url));
    versionWarning.exec();
    return true;
  }

  return false;
}

} // namespace tomviz
