// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef CORE_STYLE_BASIC_SHAPE_DATA_H_
#define CORE_STYLE_BASIC_SHAPE_DATA_H_

#include <array>

#include "base/include/value/base_string.h"
#include "core/renderer/starlight/style/css_type.h"
#include "core/renderer/starlight/types/nlength.h"

namespace lynx::starlight {

struct BasicShapeData {
  enum class CornerType { kRect, kRounded, kSuperEllipse };

  BasicShapeType type = BasicShapeType::kUnknown;
  NLength radius_x = NLength::MakeUnitNLength(0);
  NLength radius_y = NLength::MakeUnitNLength(0);
  NLength center_x = NLength::MakeUnitNLength(0);
  NLength center_y = NLength::MakeUnitNLength(0);
  // Top, right, bottom, left.
  std::array<NLength, 4> insets = {
      NLength::MakeUnitNLength(0), NLength::MakeUnitNLength(0),
      NLength::MakeUnitNLength(0), NLength::MakeUnitNLength(0)};
  // Alternating x/y radii, clockwise from the top-left corner.
  std::array<NLength, 8> corner_radii = {
      NLength::MakeUnitNLength(0), NLength::MakeUnitNLength(0),
      NLength::MakeUnitNLength(0), NLength::MakeUnitNLength(0),
      NLength::MakeUnitNLength(0), NLength::MakeUnitNLength(0),
      NLength::MakeUnitNLength(0), NLength::MakeUnitNLength(0)};
  CornerType corner_type = CornerType::kRect;
  double exponent_x = 2;
  double exponent_y = 2;
  base::String path_data;

  bool operator==(const BasicShapeData& rhs) const {
    if (type != rhs.type) return false;
    switch (type) {
      case BasicShapeType::kPath:
        return path_data == rhs.path_data;
      case BasicShapeType::kInset:
        return insets == rhs.insets && corner_type == rhs.corner_type &&
               (corner_type == CornerType::kRect ||
                (corner_radii == rhs.corner_radii &&
                 (corner_type != CornerType::kSuperEllipse ||
                  (exponent_x == rhs.exponent_x &&
                   exponent_y == rhs.exponent_y))));
      case BasicShapeType::kCircle:
      case BasicShapeType::kEllipse:
      case BasicShapeType::kSuperEllipse:
        return radius_x == rhs.radius_x && center_x == rhs.center_x &&
               center_y == rhs.center_y &&
               (type == BasicShapeType::kCircle || radius_y == rhs.radius_y) &&
               (type != BasicShapeType::kSuperEllipse ||
                (exponent_x == rhs.exponent_x && exponent_y == rhs.exponent_y));
      case BasicShapeType::kUnknown:
        return true;
    }
    return false;
  }
  bool operator!=(const BasicShapeData& rhs) const { return !(*this == rhs); }

  using AlwaysUseFlexOptionalMemSave = bool;
};

}  // namespace lynx::starlight

#endif  // CORE_STYLE_BASIC_SHAPE_DATA_H_
