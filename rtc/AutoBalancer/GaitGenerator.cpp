/* -*- coding:utf-8-unix; mode:c++; -*- */

#include "GaitGenerator.h"
namespace rats
{
#ifndef rad2deg
#define rad2deg(rad) (rad * 180 / M_PI)
#endif
#ifndef deg2rad
#define deg2rad(deg) (deg * M_PI / 180)
#endif
  /* member function implementation for refzmp_generator */
  void gait_generator::refzmp_generator::push_refzmp_from_footstep_list_for_dual (const std::vector<step_node>& fnl,
                                                                                  const coordinates& _support_leg_coords,
                                                                                  const coordinates& _swing_leg_coords)
  {
    hrp::Vector3 rzmp;
    hrp::Vector3 dz0, dz1, ret_zmp;
    leg_type spl = (fnl[fs_index].l_r == RLEG) ? LLEG :RLEG;
    dz0 = _support_leg_coords.rot * default_zmp_offsets[spl];
    dz1 = _swing_leg_coords.rot * default_zmp_offsets[spl == RLEG ? LLEG : RLEG];
    dz0 += _support_leg_coords.pos;
    dz1 += _swing_leg_coords.pos;
    rzmp = (dz0 + dz1) / 2.0;
    refzmp_cur_list.push_back( rzmp );
    foot_x_axis_list.push_back( hrp::Vector3(_swing_leg_coords.rot * hrp::Vector3::UnitX()) );
    swing_leg_list.push_back( fnl[fs_index].l_r );
    //std::cerr << "double " << (fnl[fs_index].l_r==RLEG?LLEG:RLEG) << " [" << refzmp_cur_list.back()(0) << " " << refzmp_cur_list.back()(1) << " " << refzmp_cur_list.back()(2) << "]" << std::endl;
    fs_index++;
  };

  void gait_generator::refzmp_generator::push_refzmp_from_footstep_list_for_single (const std::vector<step_node>& fnl)
  {
    hrp::Vector3 rzmp;
    coordinates tmp(fnl[fs_index-1].worldcoords);
    rzmp = tmp.rot * default_zmp_offsets[fnl[fs_index-1].l_r] + tmp.pos;
    refzmp_cur_list.push_back( rzmp );
    foot_x_axis_list.push_back( hrp::Vector3(tmp.rot * hrp::Vector3::UnitX()) );
    swing_leg_list.push_back( fnl[fs_index-1].l_r == RLEG ? LLEG : RLEG);
    //std::cerr << "single " << fnl[fs_index-1].l_r << " [" << refzmp_cur_list.back()(0) << " " << refzmp_cur_list.back()(1) << " " << refzmp_cur_list.back()(2) << "]" << std::endl;
    if (fs_index < fnl.size()) fs_index++;
  };

  void gait_generator::refzmp_generator::calc_current_refzmp (hrp::Vector3& ret, hrp::Vector3& swing_foot_zmp_offset, const double default_double_support_ratio, const size_t one_step_len) const
  {
    size_t cnt = one_step_len - refzmp_count; // current counter (0 -> one_step_len)
    size_t margine_count = (0.5 * default_double_support_ratio) * one_step_len;
    swing_foot_zmp_offset = default_zmp_offsets[swing_leg_list[refzmp_index]];
    double zmp_diff = 0.0; // difference between total swing_foot_zmp_offset and default_zmp_offset
    //if (cnt==0) std::cerr << "z " << refzmp_index << " " << refzmp_cur_list.size() << " " << fs_index << " " << (refzmp_index == refzmp_cur_list.size()-2) << " " << is_final_double_support_set << std::endl;

    // Calculate swing foot zmp offset for toe heel zmp transition
    if (use_toe_heel_transition &&
        !(is_start_double_support_phase() || is_end_double_support_phase())) { // Do not use toe heel zmp transition during start and end double support period because there is no swing foot
        if (thp_ptr->is_between_phases(cnt, SOLE0)) {
            double ratio = thp_ptr->calc_phase_ratio(cnt+1, SOLE0);
            swing_foot_zmp_offset(0) = (1-ratio)*swing_foot_zmp_offset(0) + ratio*toe_zmp_offset_x;
        } else if (thp_ptr->is_between_phases(cnt, HEEL2SOLE, SOLE2)) {
            double ratio = thp_ptr->calc_phase_ratio(cnt, HEEL2SOLE, SOLE2);
            swing_foot_zmp_offset(0) = ratio*swing_foot_zmp_offset(0) + (1-ratio)*heel_zmp_offset_x;
        } else if (thp_ptr->is_between_phases(cnt, SOLE0, SOLE2TOE)) {
            swing_foot_zmp_offset(0) = toe_zmp_offset_x;
        } else if (thp_ptr->is_between_phases(cnt, SOLE2HEEL, HEEL2SOLE)) {
            swing_foot_zmp_offset(0) = heel_zmp_offset_x;
        } else if (thp_ptr->is_between_phases(cnt, SOLE2TOE, SOLE2HEEL)) {
            double ratio = thp_ptr->calc_phase_ratio(cnt, SOLE2TOE, SOLE2HEEL);
            swing_foot_zmp_offset(0) = ratio * heel_zmp_offset_x + (1-ratio) * toe_zmp_offset_x;
        }
        zmp_diff = swing_foot_zmp_offset(0)-default_zmp_offsets[swing_leg_list[refzmp_index]](0);
    }

    // Calculate total reference ZMP
    if ( cnt < margine_count ) { // Start double support period
      hrp::Vector3 current_support_zmp = refzmp_cur_list[refzmp_index];
      hrp::Vector3 prev_support_zmp = (is_start_double_support_phase() ? refzmp_cur_list[refzmp_index] : refzmp_cur_list[refzmp_index-1]);
      if ( !(is_start_double_support_phase() || is_end_double_support_phase()) ) {
          // "* 0.5" is for double supprot period
          prev_support_zmp +=  ((refzmp_index == 1) ? zmp_diff*0.5: zmp_diff) * foot_x_axis_list[refzmp_index-1];
      }
      double ratio = (0.5 / margine_count) * (margine_count-cnt);
      ret = (1 - ratio) * current_support_zmp + ratio * prev_support_zmp;
    } else if ( cnt > one_step_len - margine_count ) { // End double support period
      hrp::Vector3 current_support_zmp = (is_end_double_support_phase() ? refzmp_cur_list[refzmp_index] : refzmp_cur_list[refzmp_index+1]);
      hrp::Vector3 prev_support_zmp = refzmp_cur_list[refzmp_index];
      if ( !(is_start_double_support_phase() || is_end_double_support_phase()) ) {
          // "* 0.5" is for double supprot period
          current_support_zmp += (((refzmp_index == refzmp_cur_list.size()-2) && is_final_double_support_set) ? zmp_diff * 0.5 : zmp_diff) * foot_x_axis_list[refzmp_index+1];
      }
      double ratio = (0.5 / margine_count) * (cnt - 1 - (one_step_len - margine_count));
      ret = (1 - ratio) * prev_support_zmp + ratio * current_support_zmp;
    } else {
      ret = refzmp_cur_list[refzmp_index];
    }
  };

  void gait_generator::refzmp_generator::update_refzmp (const std::vector<step_node>& fnl, const size_t one_step_len)
  {
    if ( 1 <= refzmp_count ) {
      refzmp_count--;
    } else {
      //std::cerr << "fs " << fs_index << "/" << fnl.size() << " rf " << refzmp_index << "/" << refzmp_cur_list.size() << " flg " << std::endl;
      if ( fnl.size() - 1 == fs_index ) {
        push_refzmp_from_footstep_list_for_dual(fnl, fnl[fs_index-1].worldcoords, fnl[fs_index-2].worldcoords);
        is_final_double_support_set = true;
      } else if ( fnl.size () - 1 > fs_index ) {
        push_refzmp_from_footstep_list_for_single(fnl);
      }
      refzmp_index++;
      refzmp_count = one_step_len;
    }
  };

  /* member function implementation for leg_coords_generator */
  void gait_generator::leg_coords_generator::calc_current_swing_leg_coords (coordinates& ret,
                                                                            const double ratio, const double step_height)
  {
    switch (default_orbit_type) {
    case SHUFFLING:
      mid_coords(ret, ratio, swing_leg_src_coords, swing_leg_dst_coords);
      break;
    case CYCLOID:
      cycloid_midcoords(ret, ratio, swing_leg_src_coords, swing_leg_dst_coords, step_height);
      break;
    case RECTANGLE:
      rectangle_midcoords(ret, ratio, swing_leg_src_coords, swing_leg_dst_coords, step_height);
      break;
    case STAIR:
      stair_midcoords(ret, ratio, swing_leg_src_coords, swing_leg_dst_coords, step_height);
      break;
    default: break;
    }
    if (std::fabs(step_height) > 1e-3*10) {
        modif_foot_coords_for_toe_heel_phase(ret);
    }
  };

  double gait_generator::leg_coords_generator::calc_ratio_from_double_support_ratio (const double default_double_support_ratio, const size_t one_step_len)
  {
    double swing_len = (1.0 - default_double_support_ratio) * one_step_len;
    double current_swing_len = (gp_count - 0.5 * default_double_support_ratio * one_step_len);
    double tmp_ratio = 1.0 - current_swing_len / swing_len;
    double tmp_current_swing_time;
    double ret;
    if ( tmp_ratio < 0.0 ) {
      ret = 0.0;
      tmp_current_swing_time = current_swing_len * _dt - swing_len * _dt;
    } else if ( tmp_ratio > 1.0 ) {
      ret = 1.0;
      tmp_current_swing_time = current_swing_len * _dt + (default_double_support_ratio * one_step_len + one_step_len) * _dt;
    } else {
      ret = tmp_ratio;
      tmp_current_swing_time = current_swing_len * _dt;
    }
    current_swing_time[support_leg] = (gp_count + 0.5 * default_double_support_ratio * one_step_len) * _dt;
    current_swing_time[support_leg==RLEG ? LLEG : RLEG] = tmp_current_swing_time;
    //std::cerr << "sl " << support_leg << " " << current_swing_time[support_leg==RLEG?0:1] << " " << current_swing_time[support_leg==RLEG?1:0] << " " << tmp_current_swing_time << " " << gp_count << std::endl;
    return ret;
  };

  void gait_generator::leg_coords_generator::cycloid_midpoint (hrp::Vector3& ret,
                                                               const double ratio, const hrp::Vector3& start,
                                                               const hrp::Vector3& goal, const double height) const
  {
    hrp::Vector3 u ( goal - start );
    hrp::Vector3 uz (0,0, ratio * u(2));
    u(2) = 0.0;
    double pth = 2 * M_PI * ratio, norm_u = u.norm();
    if ( !eps_eq(norm_u, 0.0,1e-3*0.01) )
      u =  u.normalized();
    /* check ratio vs 0.5 for default_top_ratio blending */
    hrp::Vector3 cycloid_point( ((0.5 > ratio) ? ( 2 * default_top_ratio * norm_u ) : ( 2 * (1 - default_top_ratio) * norm_u )) * ( pth - sin(pth) ) / (2 * M_PI) -
			   ((0.5 > ratio) ? 0.0 : (norm_u * (1 - 2 * default_top_ratio)) ), // local x
			   0, // local y
			   ( 0.5 * height * ( 1 - cos(pth) )) ); // local z
    hrp::Vector3 v(hrp::Vector3(0,0,1).cross(u));
    hrp::Matrix33 dvm;
    dvm << u(0), v(0), 0,
      u(1), v(1), 0,
      u(2), v(2), 1;
    ret = dvm * cycloid_point + start + uz;
  };

  double gait_generator::leg_coords_generator::calc_interpolated_toe_heel_angle (const toe_heel_phase start_phase, const toe_heel_phase goal_phase, const double start, const double goal)
  {
      double tmp_ip_ratio;
      size_t current_count = total_count - gp_count;
      if (thp_ptr->is_phase_starting(current_count, start_phase)) {
          toe_heel_interpolator->clear();
          toe_heel_interpolator->set(&start);
          toe_heel_interpolator->go(&goal, thp_ptr->calc_phase_period(start_phase, goal_phase, _dt));
      }
      if (!toe_heel_interpolator->isEmpty()) {
          toe_heel_interpolator->get(&tmp_ip_ratio, true);
      } else {
          toe_heel_interpolator->get(&tmp_ip_ratio, false);
      }
      return tmp_ip_ratio;
  };

  void gait_generator::leg_coords_generator::modif_foot_coords_for_toe_heel_phase (coordinates& org_coords)
  {
      coordinates new_coords;
      size_t current_count = total_count - gp_count;
      double dif_angle = 0.0;
      hrp::Vector3 ee_local_pivot_pos(hrp::Vector3(0,0,0));
      if ( thp_ptr->is_between_phases(current_count, SOLE0, SOLE2TOE) ) {
          dif_angle = calc_interpolated_toe_heel_angle(SOLE0, SOLE2TOE, 0.0, toe_angle);
          ee_local_pivot_pos(0) = toe_pos_offset_x;
      } else if ( thp_ptr->is_between_phases(current_count, SOLE2HEEL, HEEL2SOLE) ) {
          dif_angle = calc_interpolated_toe_heel_angle(SOLE2HEEL, HEEL2SOLE, -1 * heel_angle, 0.0);
          ee_local_pivot_pos(0) = heel_pos_offset_x;
      } else if ( thp_ptr->is_between_phases(current_count, SOLE2TOE, SOLE2HEEL) ) {
          // If SOLE1 phase does not exist, interpolate toe => heel smoothly, without 0 velocity phase.
          if ( thp_ptr->is_no_SOLE1_phase() ) {
              dif_angle = calc_interpolated_toe_heel_angle(SOLE2TOE, SOLE2HEEL, toe_angle, -1 * heel_angle);
              double tmpd = (-1*heel_angle-toe_angle);
              if (std::fabs(tmpd) > 1e-5) {
                  ee_local_pivot_pos(0) = (heel_pos_offset_x - toe_pos_offset_x) * (dif_angle - toe_angle) / tmpd + toe_pos_offset_x;
              }
          } else {
              if ( thp_ptr->is_between_phases(current_count, SOLE2TOE, TOE2SOLE) ) {
                  dif_angle = calc_interpolated_toe_heel_angle(SOLE2TOE, TOE2SOLE, toe_angle, 0.0);
                  ee_local_pivot_pos(0) = toe_pos_offset_x;
              } else if ( thp_ptr->is_between_phases(current_count, SOLE1, SOLE2HEEL) ) {
                  dif_angle = calc_interpolated_toe_heel_angle(SOLE1, SOLE2HEEL, 0.0, -1 * heel_angle);
                  ee_local_pivot_pos(0) = heel_pos_offset_x;
              }
          }
      }
      foot_dif_rot_angle = (dif_angle > 0.0 ? deg2rad(dif_angle) : 0.0);
      if (use_toe_joint && dif_angle > 0.0) dif_angle = 0.0;
      Eigen::AngleAxis<double> tmpr(deg2rad(dif_angle), hrp::Vector3::UnitY());
      rotm3times(new_coords.rot, org_coords.rot, tmpr.toRotationMatrix());
      new_coords.pos = org_coords.pos + org_coords.rot * ee_local_pivot_pos - new_coords.rot * ee_local_pivot_pos;
      org_coords = new_coords;
  };

  void gait_generator::leg_coords_generator::cycloid_midcoords (coordinates& ret,
                                                                const double ratio, const coordinates& start,
                                                                const coordinates& goal, const double height) const
  {
    mid_coords(ret, ratio, start, goal);
    cycloid_midpoint (ret.pos, ratio, start.pos, goal.pos, height);
  };

  void gait_generator::leg_coords_generator::rectangle_midcoords (coordinates& ret,
                                                                  const double ratio, const coordinates& start,
                                                                  const coordinates& goal, const double height)
  {
    mid_coords(ret, ratio, start, goal);
    rdtg.get_trajectory_point(ret.pos, hrp::Vector3(start.pos), hrp::Vector3(goal.pos), height);
  };

  void gait_generator::leg_coords_generator::stair_midcoords (coordinates& ret,
                                                              const double ratio, const coordinates& start,
                                                              const coordinates& goal, const double height)
  {
    mid_coords(ret, ratio, start, goal);
    sdtg.get_trajectory_point(ret.pos, hrp::Vector3(start.pos), hrp::Vector3(goal.pos), height);
  };

  void gait_generator::leg_coords_generator::update_leg_coords (const std::vector<step_node>& fnl, const double default_double_support_ratio, const size_t one_step_len, const bool force_height_zero)
  {
    if (!foot_ratio_interpolator->isEmpty()) {
        foot_ratio_interpolator->get(&rot_ratio, true);
    }
    if ( 0 == gp_index ) {
      swing_leg_dst_coords = fnl[gp_index].worldcoords;
      support_leg = fnl[gp_index+1].l_r;
    } else if (gp_index < fnl.size() - 1) {
      swing_leg_dst_coords = fnl[gp_index].worldcoords;
      support_leg_coords = fnl[gp_index-1].worldcoords;
      support_leg = fnl[gp_index-1].l_r;
    } else {
      swing_leg_dst_coords = fnl[fnl.size()-1].worldcoords;
      support_leg_coords = fnl[fnl.size()-2].worldcoords;
      support_leg = fnl[fnl.size()-2].l_r;
    }
    swing_ratio = calc_ratio_from_double_support_ratio(default_double_support_ratio, one_step_len);
    calc_current_swing_leg_coords(swing_leg_coords, swing_ratio, current_step_height);
    if ( 1 <= gp_count ) {
      gp_count--;
    } else {
      //std::cerr << "gp " << gp_index << std::endl;
      if (gp_index < fnl.size() - 1) {
        swing_leg_src_coords = support_leg_coords;
        gp_index++;
      }
      if (gp_index < fnl.size() - 1) {
        if (force_height_zero) current_step_height = 0.0;
        else current_step_height = fnl[gp_index].step_height;
      } else {
        current_step_height = 0.0;
      }
      gp_count = one_step_len;
      rdtg.reset(one_step_len, default_double_support_ratio);
      sdtg.reset(one_step_len, default_double_support_ratio);
      reset_foot_ratio_interpolator(one_step_len);
    }
  };

  /* member function implementation for gait_generator */
  void gait_generator::initialize_gait_parameter (const hrp::Vector3& cog,
                                                  const coordinates& initial_support_leg_coords,
                                                  const coordinates& initial_swing_leg_dst_coords,
                                                  const double delay)
  {
    /* clear all gait_parameter */
    one_step_len = default_step_time / dt;
    finalize_count = 0;
    footstep_node_list[0].worldcoords = initial_swing_leg_dst_coords;
    rg.reset(one_step_len);
    rg.push_refzmp_from_footstep_list_for_dual(footstep_node_list, initial_support_leg_coords, initial_swing_leg_dst_coords);
    if ( preview_controller_ptr != NULL ) {
      delete preview_controller_ptr;
      preview_controller_ptr = NULL;
    }
    //preview_controller_ptr = new preview_dynamics_filter<preview_control>(dt, cog(2) - refzmp_cur_list[0](2), refzmp_cur_list[0]);
    preview_controller_ptr = new preview_dynamics_filter<extended_preview_control>(dt, cog(2) - rg.get_refzmp_cur()(2), rg.get_refzmp_cur(), gravitational_acceleration);
    lcg.reset(one_step_len, initial_swing_leg_dst_coords, initial_swing_leg_dst_coords, initial_support_leg_coords, default_double_support_ratio);
    /* make another */
    rg.push_refzmp_from_footstep_list_for_single(footstep_node_list);
    emergency_flg = IDLING;
  };

  bool gait_generator::proc_one_tick ()
  {
    hrp::Vector3 rzmp, sfzo;
    bool refzmp_exist_p = rg.get_current_refzmp(rzmp, sfzo, default_double_support_ratio, one_step_len);
    if (!refzmp_exist_p) {
      finalize_count++;
      rzmp = prev_que_rzmp;
      sfzo = prev_que_sfzo;
    } else {
      prev_que_rzmp = rzmp;
      prev_que_sfzo = sfzo;
    }
    bool solved = preview_controller_ptr->update(refzmp, cog, swing_foot_zmp_offset, rzmp, sfzo, (refzmp_exist_p || finalize_count < preview_controller_ptr->get_delay()-default_step_time/dt));
    /* update refzmp */
    if ( lcg.get_gp_index() > 0 && lcg.get_gp_count() == static_cast<size_t>(one_step_len / 2) - 1 ) {
      if (velocity_mode_flg != VEL_IDLING) {
        std::vector<coordinates> cv;
        calc_next_coords_velocity_mode(cv, lcg.get_gp_index() + 1);
        if (velocity_mode_flg == VEL_ENDING) velocity_mode_flg = VEL_IDLING;
        overwrite_refzmp_queue(cv);
      } else if (emergency_flg == EMERGENCY_STOP) {
        std::vector<coordinates> cv;
        cv.push_back(footstep_node_list[lcg.get_gp_index()-1].worldcoords);
        cv.push_back(footstep_node_list[lcg.get_gp_index()].worldcoords);
        cv.push_back(footstep_node_list[lcg.get_gp_index()-1].worldcoords);
        overwrite_refzmp_queue(cv);
        emergency_flg = STOPPING;
      }
    }
    rg.update_refzmp(footstep_node_list, one_step_len);
    // { // debug
    //   double cart_zmp[3];
    //   preview_controller_ptr->get_cart_zmp(cart_zmp);
    //   std::cerr << "(list " << std::endl;
    //   std::cerr << ":cog "; print_vector(std::cerr, cog);
    //   std::cerr << ":refzmp "; print_vector(std::cerr, refzmp);
    //   std::cerr << ":cart-zmp "; print_vector(std::cerr, cart_zmp, 3);
    //   std::cerr << ")" << std::endl;
    // }

    /* update swing_leg_coords, support_leg_coords */
    if ( solved ) {
      lcg.update_leg_coords(footstep_node_list, default_double_support_ratio, one_step_len, (emergency_flg == STOPPING));
    }
    return solved;
  };

  /* generate vector of step_node from :go-pos params
   *  x, y and theta are simply divided by using stride params
   *  unit system -> x [mm], y [mm], theta [deg]
   */
  void gait_generator::go_pos_param_2_footstep_list (const double goal_x, const double goal_y, const double goal_theta,
                                                     const coordinates& _foot_midcoords, const leg_type start_leg)
  {
    coordinates foot_midcoords(_foot_midcoords); /* foot_midcoords is modified during loop */
    coordinates goal_foot_midcoords(_foot_midcoords);
    goal_foot_midcoords.pos += goal_foot_midcoords.rot * hrp::Vector3(goal_x, goal_y, 0.0);
    goal_foot_midcoords.rotate(deg2rad(goal_theta), hrp::Vector3(0,0,1));
    std::cerr << "current foot midcoords" << std::endl;
    std::cerr << "  pos =" << std::endl;
    std::cerr << _foot_midcoords.pos.format(Eigen::IOFormat(Eigen::StreamPrecision, 0, ", ", ", ", "", "", "    [", "]")) << std::endl;
    std::cerr << "  rot =" << std::endl;
    std::cerr << _foot_midcoords.rot.format(Eigen::IOFormat(Eigen::StreamPrecision, 0, ", ", "\n", "    [", "]")) << std::endl;
    std::cerr << "goal foot midcoords" << std::endl;
    std::cerr << "  pos =" << std::endl;
    std::cerr << goal_foot_midcoords.pos.format(Eigen::IOFormat(Eigen::StreamPrecision, 0, ", ", ", ", "", "", "    [", "]")) << std::endl;
    std::cerr << "  rot =" << std::endl;
    std::cerr << goal_foot_midcoords.rot.format(Eigen::IOFormat(Eigen::StreamPrecision, 0, ", ", "\n", "    [", "]")) << std::endl;

    /* initialize */
    clear_footstep_node_list();
    append_go_pos_step_node(foot_midcoords, start_leg);

    /* footstep generation loop */
    hrp::Vector3 dp, dr;
    foot_midcoords.difference(dp, dr, goal_foot_midcoords);
    dp = foot_midcoords.rot.transpose() * dp;
    dr = foot_midcoords.rot.transpose() * dr;
    while ( !(eps_eq(dp.norm(), 0.0, 1e-3*0.1) && eps_eq(dr.norm(), 0.0, deg2rad(0.5))) ) {
      set_velocity_param(dp(0)/default_step_time, dp(1)/default_step_time, rad2deg(dr(2))/default_step_time);
      append_footstep_list_velocity_mode();
      foot_midcoords = footstep_node_list.back().worldcoords;
      foot_midcoords.pos += foot_midcoords.rot * hrp::Vector3(footstep_param.leg_default_translate_pos[footstep_node_list.back().l_r] * -1.0);
      foot_midcoords.difference(dp, dr, goal_foot_midcoords);
      dp = foot_midcoords.rot.transpose() * dp;
      dr = foot_midcoords.rot.transpose() * dr;
    }

    /* finalize */
    append_go_pos_step_node(foot_midcoords, (footstep_node_list.back().l_r == RLEG ? LLEG : RLEG));
    append_go_pos_step_node(foot_midcoords, (footstep_node_list.back().l_r == RLEG ? LLEG : RLEG));
  };

  void gait_generator::go_single_step_param_2_footstep_list (const double goal_x, const double goal_y, const double goal_z, const double goal_theta,
                                                             const std::string& tmp_swing_leg,
                                                             const coordinates& _support_leg_coords)
  {
    leg_type _swing_leg = (tmp_swing_leg == "rleg") ? RLEG : LLEG;
    step_node sn0((_swing_leg == RLEG) ? LLEG : RLEG, _support_leg_coords, lcg.get_default_step_height());
    footstep_node_list.push_back(sn0);
    step_node sn1(_swing_leg, _support_leg_coords, lcg.get_default_step_height());
    hrp::Vector3 trs(2.0 * footstep_param.leg_default_translate_pos[_swing_leg] + hrp::Vector3(goal_x, goal_y, goal_z));
    sn1.worldcoords.pos += sn1.worldcoords.rot * trs;
    sn1.worldcoords.rotate(deg2rad(goal_theta), hrp::Vector3(0,0,1));
    footstep_node_list.push_back(sn1);
    footstep_node_list.push_back(sn0);
  };

  void gait_generator::initialize_velocity_mode (const coordinates& _foot_midcoords,
						 const double vel_x, const double vel_y, const double vel_theta)
  {
    velocity_mode_flg = VEL_DOING;
    /* initialize */
    leg_type current_leg = (vel_y > 0.0) ? RLEG : LLEG;
    clear_footstep_node_list();
    set_velocity_param (vel_x, vel_y, vel_theta);
    append_go_pos_step_node(_foot_midcoords, current_leg);
    append_footstep_list_velocity_mode();
    append_footstep_list_velocity_mode();
    append_footstep_list_velocity_mode();
  };

  void gait_generator::finalize_velocity_mode ()
  {
    if (velocity_mode_flg == VEL_DOING) velocity_mode_flg = VEL_ENDING;
  };

  void gait_generator::calc_foot_midcoords_trans_vector_velocity_mode (coordinates& foot_midcoords, hrp::Vector3& trans, double& dth, const step_node& sn)
  {
    foot_midcoords = sn.worldcoords;
    hrp::Vector3 tmpv(footstep_param.leg_default_translate_pos[sn.l_r] * -1.0);
    foot_midcoords.pos += foot_midcoords.rot * tmpv;
    double dx = vel_param.velocity_x + offset_vel_param.velocity_x, dy = vel_param.velocity_y + offset_vel_param.velocity_y;
    dth = vel_param.velocity_theta + offset_vel_param.velocity_theta;
    /* velocity limitation by stride parameters <- this should be based on footstep candidates */
    if (footstep_param.stride_fwd_x / default_step_time < dx)
      dx = footstep_param.stride_fwd_x / default_step_time;
    if (-1*footstep_param.stride_bwd_x / default_step_time > dx)
      dx = -1*footstep_param.stride_bwd_x / default_step_time;
    if (footstep_param.stride_y / default_step_time < fabs(dy))
      dy = footstep_param.stride_y * ((dy > 0.0) ? 1.0 : -1.0) / default_step_time;
    if (footstep_param.stride_theta / default_step_time < fabs(dth))
      dth = footstep_param.stride_theta * ((dth > 0.0) ? 1.0 : -1.0) / default_step_time;
    /* inside step limitation */
    if (use_inside_step_limitation) {
      if (vel_param.velocity_y > 0) {
        if (sn.l_r == LLEG) dy *= 0.5;
      } else {
        if (sn.l_r == RLEG) dy *= 0.5;
      }
      if (vel_param.velocity_theta > 0) {
        if (sn.l_r == LLEG) dth *= 0.5;
      } else {
        if (sn.l_r == RLEG) dth *= 0.5;
      }
    }
    trans = hrp::Vector3(dx * default_step_time, dy * default_step_time, 0);
    dth = deg2rad(dth * default_step_time);
  };

  void gait_generator::append_footstep_list_velocity_mode ()
  {
    coordinates foot_midcoords;
    hrp::Vector3 trans;
    double dth;
    calc_foot_midcoords_trans_vector_velocity_mode(foot_midcoords, trans, dth, footstep_node_list.back());

    foot_midcoords.pos += foot_midcoords.rot * trans;
    foot_midcoords.rotate(dth, hrp::Vector3(0,0,1));
    append_go_pos_step_node(foot_midcoords, (( footstep_node_list.back().l_r == RLEG ) ? LLEG : RLEG));
  };

  void gait_generator::calc_next_coords_velocity_mode (std::vector<coordinates>& ret, const size_t idx)
  {
    coordinates foot_midcoords;
    hrp::Vector3 trans;
    double dth;
    calc_foot_midcoords_trans_vector_velocity_mode(foot_midcoords, trans, dth, footstep_node_list[idx-1]);

    for (size_t i = 0; i < 3; i++) {
      ret.push_back(foot_midcoords);
      if ( velocity_mode_flg != VEL_ENDING ) {
        ret[i].pos += ret[i].rot * trans;
        ret[i].rotate(dth, hrp::Vector3(0,0,1));
      }
      ret[i].pos += ret[i].rot * footstep_param.leg_default_translate_pos[(footstep_node_list[idx-1].l_r == RLEG) ? (1 + i)%2 : i%2];
    }
  };

  void gait_generator::overwrite_refzmp_queue(const std::vector<coordinates>& cv)
  {
    /* clear footstep and refzmp after gp_index + 1, it means we do not modify current step */
    size_t idx = lcg.get_gp_index() + 1;
    /* reset index and counter */
    rg.set_indices(idx);
    rg.set_refzmp_count(one_step_len);

    /* add new next steps ;; the number of next steps is cv.size() */
    for (size_t i = 0; i < cv.size(); i++ ) {
      if ( footstep_node_list.size() - 1 >= idx + i) /* if footstep_node_list[idx] and footstep_node_list[idx+1]  exists */
        footstep_node_list[idx + i].worldcoords = cv[i];
      else
        footstep_node_list.push_back(step_node(footstep_node_list[lcg.get_gp_index()-1 + i].l_r, cv[i], lcg.get_default_step_height()));
    }

    /* remove steps after newly added steps */
    while ( footstep_node_list.size() > idx + cv.size())//
      footstep_node_list.pop_back();
    /* remove refzmp after idx for allocation of new refzmp by push_refzmp_from_footstep_list */
    rg.remove_refzmp_cur_list_over_length(idx);
    /* remove refzmp in preview contoroller queue */
    preview_controller_ptr->remove_preview_queue(lcg.get_gp_count());

    /* reset refzmp */
    for (size_t i = 0; i < cv.size()-1; i++) {
      if (emergency_flg == EMERGENCY_STOP)
        rg.push_refzmp_from_footstep_list_for_dual(footstep_node_list, cv[i%2], cv[(i+1)%2]);
      else
        rg.push_refzmp_from_footstep_list_for_single(footstep_node_list);
    }
    if (emergency_flg == EMERGENCY_STOP)
      rg.push_refzmp_from_footstep_list_for_dual(footstep_node_list, cv[0], cv[1]);
    /* fill preview controller queue by new refzmp */
    hrp::Vector3 rzmp, sfzo;
    bool not_solved = true;
    while (not_solved) {
      bool refzmp_exist_p = rg.get_current_refzmp(rzmp, sfzo, default_double_support_ratio, one_step_len);
      not_solved = !preview_controller_ptr->update(refzmp, cog, swing_foot_zmp_offset, rzmp, sfzo, refzmp_exist_p);
      rg.update_refzmp(footstep_node_list, one_step_len);
    }
  };
}

