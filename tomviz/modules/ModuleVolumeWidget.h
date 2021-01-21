/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleVolumeWidget_h
#define tomvizModuleVolumeWidget_h

#include <QScopedPointer>
#include <QWidget>

class QFormLayout;

/**
 * \brief UI layer of ModuleVolume.
 *
 * Signals are forwarded to the actuators on the mapper in ModuleVolume.
 * This class is intended to contain only logic related to UI actions.
 */

namespace Ui {
class ModuleVolumeWidget;
class LightingParametersForm;
} // namespace Ui

namespace tomviz {

class ModuleVolumeWidget : public QWidget
{
  Q_OBJECT

public:
  ModuleVolumeWidget(QWidget* parent_ = nullptr);
  ~ModuleVolumeWidget() override;

  //@{
  /**
   * UI update methods. The actual model state is stored in ModelVolume (either
   * in the mapper or serialized), so the UI needs to be updated if the state
   * changes or when constructing the UI.
   */
  void setActiveScalars(const QString& scalars);
  void setJittering(const bool enable);
  void setBlendingMode(const int mode);
  void setInterpolationType(const int type);
  void setLighting(const bool enable);
  void setAmbient(const double value);
  void setDiffuse(const double value);
  void setSpecular(const double value);
  void setSpecularPower(const double value);
  void setTransferMode(const int transferMode);
  void setSolidity(const double value);
  void setRgbaMappingAllowed(const bool allowed);
  void setUseRgbaMapping(const bool b);
  void setRgbaMappingMin(const double value);
  void setRgbaMappingMax(const double value);
  void setRgbaMappingSliderRange(const double range[2]);
  QFormLayout* formLayout();
  //@}

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void jitteringToggled(const bool state);
  void blendingChanged(const int state);
  void interpolationChanged(const int state);
  void lightingToggled(const bool state);
  void ambientChanged(const double value);
  void diffuseChanged(const double value);
  void specularChanged(const double value);
  void specularPowerChanged(const double value);
  void transferModeChanged(const int mode);
  void solidityChanged(const double value);
  void useRgbaMappingToggled(const bool b);
  void rgbaMappingMinChanged(const double value);
  void rgbaMappingMaxChanged(const double value);
  //@}

private:
  ModuleVolumeWidget(const ModuleVolumeWidget&) = delete;
  void operator=(const ModuleVolumeWidget&) = delete;

  bool usesLighting(const int mode) const;

  QScopedPointer<Ui::ModuleVolumeWidget> m_ui;
  QScopedPointer<Ui::LightingParametersForm> m_uiLighting;

private slots:
  void onBlendingChanged(const int mode);
  void onRgbaMappingMinChanged(double value);
  void onRgbaMappingMaxChanged(double value);
};
} // namespace tomviz
#endif
