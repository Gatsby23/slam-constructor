#ifndef SLAM_CTOR_SLAMS_VINY_SLAM_H
#define SLAM_CTOR_SLAMS_VINY_SLAM_H

#include <memory>

#include "../../utils/init_scan_matching.h"
#include "../../utils/init_occupancy_mapping.h"

#include "../../core/maps/plain_grid_map.h"
#include "../../core/states/single_state_hypothesis_laser_scan_grid_world.h"

#include "viny_scan_probability_estimator.h"
#include "viny_grid_cell.h"

using VinySlam= SingleStateHypothesisLaserScanGridWorld<UnboundedPlainGridMap>;

auto init_viny_slam(const PropertiesProvider &props) {
  auto slam_props = SingleStateHypothesisLSGWProperties{};
  // FIXME: move to params
  slam_props.localized_scan_quality = 0.9;
  slam_props.raw_scan_quality = 0.6;
  slam_props.cell_prototype = std::make_shared<VinyDSCell>();

  auto spe = std::make_shared<VinyScanProbabilityEstimator>(init_oope(props));
  slam_props.gsm = init_scan_matcher(props, spe);

  // TODO: move to init utils, rename to map distorsion
  slam_props.gmsa = std::make_shared<WallDistanceBlurringScanAdder>(
    init_occ_estimator(props), init_mapping_blur(props));
  slam_props.map_props = init_grid_map_params(props);
  return std::make_shared<VinySlam>(slam_props);
}

#endif
