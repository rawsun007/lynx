// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "core/renderer/dom/fragment/clip_path_resolver.h"

#include <cmath>

#include "core/renderer/starlight/style/css_type.h"
namespace lynx::tasm {
namespace {
float Length(const starlight::NLength& length, float reference, bool& depends) {
  depends |= length.NumericLength().ContainsPercentage() &&
             length.NumericLength().GetPercentagePart() != 0;
  return starlight::NLengthToLayoutUnit(length,
                                        starlight::LayoutUnit(reference))
      .ToFloat();
}

bool ValidPosition(const ResolvedClipPath& clip) {
  return std::isfinite(clip.x) && std::isfinite(clip.y);
}

fml::RefPtr<gfx::Path> MakeInsetPath(const starlight::BasicShapeData& shape,
                                     float width, float height) {
  if (width <= 0 || height <= 0) {
    // Check before replacing dimensions with zero, which would hide overflow.
    if (!std::isfinite(width) || !std::isfinite(height)) return nullptr;
    return gfx::Path::MakeRect(0, 0);
  }
  if (shape.corner_type == starlight::BasicShapeData::CornerType::kRect)
    return gfx::Path::MakeRect(width, height);
  const bool super =
      shape.corner_type == starlight::BasicShapeData::CornerType::kSuperEllipse;
  // Insets already depend on both box dimensions, even for fixed lengths.
  bool depends = true;
  float radii[8];
  for (size_t i = 0; i < 8; ++i) {
    // Corner percentages resolve against the inset rectangle, then all
    // corners share the CSS overlap normalization factor.
    radii[i] = Length(shape.corner_radii[i], i % 2 ? height : width, depends);
    // Clamp finite negative radii; leave infinity for the Path factory to
    // reject.
    if (radii[i] < 0 && std::isfinite(radii[i])) radii[i] = 0;
  }
  if (!super) return gfx::Path::MakeRoundedRect(width, height, radii);
  return gfx::Path::MakeSuperRoundedRect(width, height, radii, shape.exponent_x,
                                         shape.exponent_y);
}
}  // namespace
ResolvedClipPath ResolveClipPath(const starlight::BasicShapeData* computed,
                                 float width, float height, float scale) {
  ResolvedClipPath out;
  if (!computed) return out;
  const auto invalid = [&out]() {
    out.status = ClipPathStatus::kInvalid;
    out.path = gfx::Path::MakeRect(0, 0);
    out.x = out.y = 0;
    return out;
  };
  const auto& shape = *computed;
  if (!std::isfinite(width) || !std::isfinite(height) || width < 0 ||
      height < 0)
    return invalid();
  auto type = shape.type;
  if (type == starlight::BasicShapeType::kPath) {
    out.depends_on_source_scale = true;
    out.path = gfx::Path::MakeSvgPathData(shape.path_data, scale);
  } else if (type == starlight::BasicShapeType::kCircle) {
    float diagonal = static_cast<float>(
        std::hypot(static_cast<double>(width), height) / std::sqrt(2.));
    float radius = Length(shape.radius_x, diagonal, out.depends_on_width);
    out.depends_on_height = out.depends_on_width;
    out.x = Length(shape.center_x, width, out.depends_on_width);
    out.y = Length(shape.center_y, height, out.depends_on_height);
    if (!ValidPosition(out)) return invalid();
    out.path = gfx::Path::MakeEllipse(radius, radius);
  } else if (type == starlight::BasicShapeType::kEllipse ||
             type == starlight::BasicShapeType::kSuperEllipse) {
    bool super = type == starlight::BasicShapeType::kSuperEllipse;
    float rx = Length(shape.radius_x, width, out.depends_on_width);
    float ry = Length(shape.radius_y, height, out.depends_on_height);
    out.x = Length(shape.center_x, width, out.depends_on_width);
    out.y = Length(shape.center_y, height, out.depends_on_height);
    if (!ValidPosition(out)) return invalid();
    if (super) {
      out.path = gfx::Path::MakeSuperEllipse(rx, ry, shape.exponent_x,
                                             shape.exponent_y);
    } else
      out.path = gfx::Path::MakeEllipse(rx, ry);
  } else if (type == starlight::BasicShapeType::kInset) {
    // Even absolute inset distances leave the far edges tied to the box.
    out.depends_on_width = out.depends_on_height = true;
    float top = Length(shape.insets[0], height, out.depends_on_height);
    float right = Length(shape.insets[1], width, out.depends_on_width);
    float bottom = Length(shape.insets[2], height, out.depends_on_height);
    float left = Length(shape.insets[3], width, out.depends_on_width);
    float w = width - left - right, h = height - top - bottom;
    out.x = left;
    out.y = top;
    out.path = MakeInsetPath(shape, w, h);
  } else
    return invalid();
  if (!out.path) return invalid();
  out.status = ClipPathStatus::kValid;
  return out;
}
}  // namespace lynx::tasm
