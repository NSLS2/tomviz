/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizFxiFormat_h
#define tomvizFxiFormat_h

#include <string>

#include <QVariantMap>

#include "core/Variant.h"

class vtkImageData;

namespace tomviz {

class DataSource;

/**
 * Format used by the FXI beamline at BNL.
 */

class FxiFormat
{
public:
  // This will only read /exchange/data, nothing else
  bool read(const std::string& fileName, vtkImageData* data,
            const QVariantMap& options = QVariantMap());
  // This will read the data as well as dark, white, and the
  // theta angles, and it will swap x and z for tilt series.
  bool read(const std::string& fileName, DataSource* source,
            const QVariantMap& options = QVariantMap());

private:
  // Read the dark dataset into the image data
  bool readDark(const std::string& fileName, vtkImageData* data,
                const QVariantMap& options = QVariantMap());
  // Read the white dataset into the image data
  bool readWhite(const std::string& fileName, vtkImageData* data,
                 const QVariantMap& options = QVariantMap());
  // Read the theta angles from /exchange/theta
  QVector<double> readTheta(const std::string& fileName,
                            const QVariantMap& options = QVariantMap());
  // Read and return any known metadata from the file
  std::map<std::string, Variant> readMetadata(const std::string& fileName,
                                              const QVariantMap& options);
};
} // namespace tomviz

#endif // tomvizFxiFormat_h
