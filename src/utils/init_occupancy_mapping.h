#ifndef SLAM_CTOR_UTILS_INIT_OCCUPANCY_MAPPING_H
#define SLAM_CTOR_UTILS_INIT_OCCUPANCY_MAPPING_H

#include <memory>
#include <limits>
#include <tuple>

#include "properties_providers.h"

#include "../core/maps/grid_map.h"
#include "../core/maps/plain_grid_map.h"
#include "../core/maps/lazy_tiled_grid_map.h"
#include "../core/maps/grid_map_scan_adders.h"
#include "../core/maps/tbm_grid_cells.h"
#include "../core/maps/naive_grid_cells.h"
#include "../core/maps/area_occupancy_estimator.h"
#include "../core/maps/const_occupancy_estimator.h"

#include "init_scan_matching.h"
#include "../core/scan_matchers/m3rsm_engine.h"

auto init_pose_quality_estimators(const PropertiesProvider &props) {
  static const auto MAPPING_NS = std::string{"slam/mapping/"};

  auto corrected_pose_quality = props.get_dbl(
    MAPPING_NS + "corrected_pose_quality", 1.0);
  auto odometry_pose_quality = props.get_dbl(
    MAPPING_NS + "raw_pose_quality", 0.5);
  return std::make_tuple(corrected_pose_quality, odometry_pose_quality);
}

auto init_grid_map_params(const PropertiesProvider &props) {
  static const std::string Map_NS = "slam/map/";
  auto w = props.get_dbl(Map_NS + "height_in_meters", 10);
  auto h = props.get_dbl(Map_NS + "width_in_meters", 10);
  auto scale = props.get_dbl(Map_NS + "meters_per_cell", 0.1);
  return GridMapParams{static_cast<int>(std::ceil(w / scale)),
                       static_cast<int>(std::ceil(h / scale)),
                       scale};
}

std::shared_ptr<CellOccupancyEstimator> init_occ_estimator(
    const PropertiesProvider &props) {

  // TODO: replace with "slam/mapping" after the config refactoring
  static const auto MAPPING_NS = std::string{"slam/"};
  static const auto COE_NS = MAPPING_NS + "occupancy_estimator/";
  auto base_occ = Occupancy{props.get_dbl(COE_NS + "base_occupied/prob", 0.95),
                            props.get_dbl(COE_NS + "base_occupied/qual", 1.0)};
  auto base_empty = Occupancy{props.get_dbl(COE_NS + "base_empty/prob", 0.01),
                              props.get_dbl(COE_NS + "base_empty/qual", 1.0)};

  auto type = props.get_str(COE_NS + "type", "const");
  if (type == "const") {
    return std::make_shared<ConstOccupancyEstimator>(base_occ, base_empty);
  } else if (type == "area") {
    return std::make_shared<AreaOccupancyEstimator>(base_occ, base_empty);
  } else {
    std::cerr << "Unknown estimator type: " << type << std::endl;
    std::exit(-1);
  }
}

std::shared_ptr<ObservationMappingQualityEstimator> init_omqe(
    const PropertiesProvider &props) {

  static const auto MAPPING_NS = std::string{"slam/mapping/"};
  static const auto OMQE_NS = MAPPING_NS + "observation_quality_estimator/type";
  // TODO: replace with <undefined> after config refactoring
  auto type = props.get_str(OMQE_NS + "type", "idle");
  std::cout << "Used OMQE: " << type << std::endl;
  if (type == "idle") {
    return std::make_shared<IdleOMQE>();
  } else if (type == "ahr") {
    return std::make_shared<AngleHistogramResiprocalOMQE>();
  } else {
    std::cerr << "[ERROR] Unknown OMQE type: " << type << std::endl;
    std::exit(-1);
  }
}

auto init_scan_adder(const PropertiesProvider &props) {
  static const auto DBL_INF = std::numeric_limits<double>::infinity();
  auto builder = WallDistanceBlurringScanAdder::builder();
  return
    builder.set_occupancy_estimator(init_occ_estimator(props))
           .set_observation_quality_estimator(init_omqe(props))
           .set_blur_distance(props.get_dbl("slam/mapping/blur", 0.0))
           .set_max_usable_range(props.get_dbl("slam/mapping/max_range",
                                               DBL_INF))
           .build();
}

auto init_occupied_area_model(const PropertiesProvider &props) {
  auto area_type = props.get_str("slam/mapping/grid/area/type", "<undefined>");
  auto model = std::shared_ptr<GridCell>{nullptr};

  if (area_type == "tbm_consistent") {
    model = std::make_shared<TbmOccConsistentCell>();
  } else if (area_type == "tbm_unknown_even_occ") {
    model = std::make_shared<TbmUnknownEvenOccCell>();
  } else if (area_type == "affine_quality_merge") {
    model = std::make_shared<AffineQualityMergeCell>();
  } else if (area_type == "mean_probability") {
    model = std::make_shared<MeanProbabilityCell>();
  } else {
    std::cerr << "Unknown occupied area type: " << area_type << std::endl;
    std::exit(-1);
  }
  return model;
}

template <typename MapT>
std::shared_ptr<GridMap> create_grid_map(const PropertiesProvider &props,
                                         std::shared_ptr<GridCell> area_model) {
  auto map_params = init_grid_map_params(props);
  if (is_m3rsm(scan_matcher_type(props))) {
    using M3RSM_MapT = M3RSMRescalableGridMap<MapT>;
    auto oie = init_oie(props, false);
    return std::make_shared<M3RSM_MapT>(oie, area_model, map_params);
  }

  return std::make_shared<MapT>(area_model, map_params);
}

decltype(auto) init_grid_map(const PropertiesProvider &props,
                             std::shared_ptr<GridCell> area_model) {
  auto const GM_Name = "slam/mapping/grid/type";
  auto map_type = props.get_str(GM_Name, "<undefined>");
  auto map = std::shared_ptr<GridMap>{nullptr};

  if (map_type == "plain") {
    map = create_grid_map<PlainGridMap>(props, area_model);
  } else if (map_type == "unbounded_plain") {
    map = create_grid_map<UnboundedPlainGridMap>(props, area_model);
  } else if (map_type == "lazy_tiled") {
    map = create_grid_map<LazyTiledGridMap>(props, area_model);
  } else if (map_type == "unbounded_lazy_tiled") {
    map = create_grid_map<UnboundedLazyTiledGridMap>(props, area_model);
  } else {
    std::cerr << "Unknown grid map type (" << GM_Name << "): "
              << map_type << std::endl;
    std::exit(-1);
  }
  return map;
}

decltype(auto) init_grid_map(const PropertiesProvider &props) {
  return init_grid_map(props, init_occupied_area_model(props));
}

#endif
