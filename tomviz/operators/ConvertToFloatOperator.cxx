/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ConvertToFloatOperator.h"

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

namespace {

template <typename T>
void convertToFloat(vtkFloatArray* fArray, int nComps, vtkIdType nTuples,
                    void* data)
{
  auto d = static_cast<T*>(data);
  auto a = static_cast<float*>(fArray->GetVoidPointer(0));
  for (vtkIdType i = 0; i < nComps * nTuples; ++i) {
    a[i] = static_cast<float>(d[i]);
  }
}
} // namespace

namespace tomviz {

ConvertToFloatOperator::ConvertToFloatOperator(QObject* p) : Operator(p) {}

QIcon ConvertToFloatOperator::icon() const
{
  return QIcon();
}

bool ConvertToFloatOperator::applyTransform(vtkDataObject* data)
{
  auto imageData = vtkImageData::SafeDownCast(data);
  // sanity check
  if (!imageData) {
    return false;
  }
  auto scalars = imageData->GetPointData()->GetScalars();
  vtkNew<vtkFloatArray> floatArray;
  floatArray->SetNumberOfComponents(scalars->GetNumberOfComponents());
  floatArray->SetNumberOfTuples(scalars->GetNumberOfTuples());
  floatArray->SetName(scalars->GetName());
  switch (scalars->GetDataType()) {
    vtkTemplateMacro(convertToFloat<VTK_TT>(
      floatArray.Get(), scalars->GetNumberOfComponents(),
      scalars->GetNumberOfTuples(), scalars->GetVoidPointer(0)));
  }
  imageData->GetPointData()->RemoveArray(scalars->GetName());
  imageData->GetPointData()->SetScalars(floatArray);
  return true;
}

Operator* ConvertToFloatOperator::clone() const
{
  return new ConvertToFloatOperator();
}

} // namespace tomviz
