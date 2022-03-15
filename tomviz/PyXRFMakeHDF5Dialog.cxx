/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFMakeHDF5Dialog.h"
#include "ui_PyXRFMakeHDF5Dialog.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPointer>

namespace tomviz {

class PyXRFMakeHDF5Dialog::Internal : public QObject
{
public:
  Ui::PyXRFMakeHDF5Dialog ui;
  QPointer<PyXRFMakeHDF5Dialog> parent;

  Internal(PyXRFMakeHDF5Dialog* p) : parent(p)
  {
    ui.setupUi(p);
    setParent(p);

    // Hide the tab bar. We will change pages automatically.
    ui.methodWidget->tabBar()->hide();

    setupConnections();
  }

  void setupConnections()
  {
    connect(ui.method, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Internal::methodChanged);
    connect(ui.selectWorkingDirectory, &QPushButton::clicked, this,
            &Internal::selectWorkingDirectory);

    connect(ui.buttonBox, &QDialogButtonBox::accepted, this,
            &Internal::accepted);
  }

  bool useAlreadyExistingData() const
  {
    return ui.method->currentText() == "Already Existing";
  }

  int scanStart() const { return ui.scanStart->value(); }

  void setScanStart(int x) { ui.scanStart->setValue(x); }

  int scanStop() const { return ui.scanStop->value(); }

  void setScanStop(int x) { ui.scanStop->setValue(x); }

  bool successfulScansOnly() const
  {
    return ui.successfulScansOnly->isChecked();
  }

  void setSuccessfulScansOnly(bool b) { ui.successfulScansOnly->setChecked(b); }

  QString method() const { return ui.method->currentText(); }

  void setMethod(QString s) { ui.method->setCurrentText(s); }

  void methodChanged(int i)
  {
    // The indices match
    ui.methodWidget->setCurrentIndex(i);
  }

  QString workingDirectory() const { return ui.workingDirectory->text(); }

  void setWorkingDirectory(QString s) { ui.workingDirectory->setText(s); }

  QString defaultWorkingDirectory() const
  {
    return QDir::home().filePath("data");
  }

  void selectWorkingDirectory()
  {
    QString caption = "Select working directory";
    auto directory = QFileDialog::getExistingDirectory(parent.data(), caption,
                                                       workingDirectory());
    if (directory.isEmpty()) {
      return;
    }

    setWorkingDirectory(directory);
  }

  void accepted()
  {
    QString reason;
    if (!validate(reason)) {
      QString title = "Invalid Settings";
      QMessageBox::critical(parent.data(), title, reason);
      parent->show();
      return;
    }

    writeSettings();
    parent->accept();
  }

  bool validate(QString& reason)
  {
    auto workingDir = workingDirectory();
    if (!QDir(workingDir).exists()) {
      // First ask if the user wants to make it.
      QString title = "Directory does not exist";
      auto text = QString("Working directory \"%1\" does not exist. Create it?")
                    .arg(workingDir);
      if (QMessageBox::question(parent, title, text) == QMessageBox::Yes) {
        QDir().mkpath(workingDir);
      }
    }

    if (!useAlreadyExistingData() && !QDir(workingDir).isEmpty()) {
      QString title = "Directory is not empty";
      auto text = QString("Working directory \"%1\" is not empty. Its "
                          "contents will be removed. Proceed?")
                    .arg(workingDir);

      if (QMessageBox::question(parent, title, text) == QMessageBox::No) {
        reason = "Working directory is not empty: " + workingDir;
        return false;
      }

      QDir(workingDir).removeRecursively();
      QDir().mkpath(workingDir);
    }

    if (workingDir.isEmpty() || !QDir(workingDir).exists()) {
      reason = "Working directory does not exist: " + workingDir;
      return false;
    }

    if (scanStart() > scanStop()) {
      reason = QString("Scan start, %1, cannot be greater than scan stop, %2")
                 .arg(scanStart())
                 .arg(scanStop());
      return false;
    }

    return true;
  }

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");
    settings->beginGroup("makeHDF5");

    setMethod(settings->value("method", "New").toString());
    setWorkingDirectory(
      settings->value("workingDirectory", defaultWorkingDirectory())
        .toString());
    setScanStart(settings->value("scanStart", 0).toInt());
    setScanStop(settings->value("scanStop", 0).toInt());
    setSuccessfulScansOnly(
      settings->value("successfulScansOnly", true).toBool());

    settings->endGroup();
    settings->endGroup();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");
    settings->beginGroup("makeHDF5");

    settings->setValue("method", method());
    settings->setValue("workingDirectory", workingDirectory());
    settings->setValue("scanStart", scanStart());
    settings->setValue("scanStop", scanStop());
    settings->setValue("successfulScansOnly", successfulScansOnly());

    settings->endGroup();
    settings->endGroup();
  }
};

PyXRFMakeHDF5Dialog::PyXRFMakeHDF5Dialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
}

PyXRFMakeHDF5Dialog::~PyXRFMakeHDF5Dialog() = default;

void PyXRFMakeHDF5Dialog::show()
{
  m_internal->readSettings();
  QDialog::show();
}

bool PyXRFMakeHDF5Dialog::useAlreadyExistingData() const
{
  return m_internal->useAlreadyExistingData();
}

QString PyXRFMakeHDF5Dialog::workingDirectory() const
{
  return m_internal->workingDirectory();
}

int PyXRFMakeHDF5Dialog::scanStart() const
{
  return m_internal->scanStart();
}

int PyXRFMakeHDF5Dialog::scanStop() const
{
  return m_internal->scanStop();
}

bool PyXRFMakeHDF5Dialog::successfulScansOnly() const
{
  return m_internal->successfulScansOnly();
}

} // namespace tomviz
