/**
 * @file   sandbox/playground.cc
 *
 * @author Felix Musil <felix.musil@epfl.ch>
 *
 * @date   26 June 2019
 *
 * @brief an executable to test ideas
 *
 * Copyright © 2019 Felix Musil, COSMO (EPFL), LAMMM (EPFL)
 *
 * librascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * librascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software; see the file LICENSE. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "rascal/models/sparse_kernels.hh"
#include "rascal/models/sparse_points.hh"
#include "rascal/utils/basic_types.hh"
#include "rascal/models/kernels.hh"
#include "rascal/utils/utils.hh"
#include "rascal/representations/calculator_sorted_coulomb.hh"
#include "rascal/representations/calculator_spherical_expansion.hh"
#include "rascal/representations/calculator_spherical_invariants.hh"
#include "rascal/structure_managers/adaptor_increase_maxorder.hh"
#include "rascal/structure_managers/cluster_ref_key.hh"
#include "rascal/structure_managers/adaptor_center_contribution.hh"
#include "rascal/structure_managers/adaptor_half_neighbour_list.hh"
#include "rascal/structure_managers/adaptor_neighbour_list.hh"
#include "rascal/structure_managers/adaptor_strict.hh"
#include "rascal/structure_managers/make_structure_manager.hh"
#include "rascal/structure_managers/structure_manager_centers.hh"
#include "rascal/structure_managers/structure_manager_collection.hh"

#include <cmath>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <list>
#include <random>
#include <string>
#include <algorithm>
#include <iterator>

using namespace rascal;  // NOLINT

using Representation_t = CalculatorSphericalInvariants;
using ManagerCollection_t =
    typename TypeHolderInjector<ManagerCollection, ManagerTypeList_t>::type;
using Representation_t = CalculatorSphericalInvariants;
using Prop_t = typename Representation_t::template Property_t<Manager_t>;
using PropGrad_t = typename Representation_t::template PropertyGradient_t<Manager_t>;

constexpr static size_t ClusterLayer_{
          Manager_t::template cluster_layer_from_order<2>()};

int main() {

  std::string filename{"../reference_data/inputs/diamond_2atom_distorted.json"};

  // json kernel_hypers{
  //       {"zeta", 1}, {"target_type", "Atom"}, {"name", "Cosine"}};
  json hypers = json_io::load("../reference_data/sparse_soap_input.json");
  json adaptors = json_io::load("../reference_data/adaptor_input.json");

  // std::cout << adaptors.dump(2) << std::endl;

  // auto manager =
  //     make_structure_manager_stack<StructureManagerCenters,
  //                                  AdaptorNeighbourList,
  //                                  AdaptorCenterContribution, AdaptorStrict>(
  //         structure, adaptors);
  ManagerCollection_t managers{adaptors};
  managers.add_structures(filename, 0, 10);
  Representation_t representation{hypers};

  representation.compute(managers);

  // constexpr size_t n_centers_print{5};
  // constexpr size_t n_neigh_print{1000};
  // size_t center_count{0};

  std::cout << "Gradients are printed with: First Cartesian component, "
               "then species pairs, along the columns; n-n'-l along the rows.";
  std::cout << std::endl;

  // auto && soap_vectors{
  //     *manager->template get_property_ptr<Prop_t>(representation.get_name())};
  // auto && soap_vector_gradients{*manager->template get_property_ptr<PropGrad_t>(
  //     representation.get_gradient_name())};

  coeff_calc.compute(collection);
  std::cout.setf(std::ios::scientific);
  std::cout.precision(5);
  for (const auto & manager : collection) {
    auto & rep{*manager->template get_property<Prop_t>(coeff_calc.get_name())};
    auto X = rep.get_features();


    std::cout << X << std::endl;
    std::cout <<  std::endl;
    for (auto center : manager) {
      std::cout << rep[center].get_full_vector().transpose() << std::endl;
    }

  }

  json kernel_hypers{
        {"zeta", 1}, {"target_type", "Structure"}, {"name", "GAP"}};
  SparseKernel kernel{kernel_hypers};

  auto KNM_test{kernel.compute(representation, managers, sparse_points)};
  auto KNM_der{kernel.compute_derivative(representation, managers, sparse_points)};
  auto diff = math::relative_error(KNM, KNM_test);

  std::cout << diff << std::endl;
  std::cout << "============================" << std::endl;
  std::cout << KNM<< std::endl;
  std::cout << "============================" << std::endl;
  std::cout << KNM_test<< std::endl;
  std::cout << "============================" << std::endl;
  std::cout << KNM_der<< std::endl;

  json j;
  j = sparse_points;
  std::cout << j.dump()<< std::endl;;
  auto sparse_points_b = j.get<SparsePointsBlockSparse<Representation_t>>();

  auto KNM_test_b{kernel.compute(representation, managers, sparse_points)};

  math::relative_error(KNM, KNM_test_b);
  // math::Matrix_t KNM_ref(n_centers, sparse_points.size());
  // KNM_ref = feat_ref * feat_test.transpose();
  // math::Matrix_t KNM(managers.size(), sparse_points.size());
  // auto Msps = sparse_points.species_by_points();
  // int i_row{0};
  // for (auto manager : managers) {
  //   for (auto center : manager) {
  //     int i_col{0};
  //     for (auto sp : Msps) {
  //       if (sp != center.get_atom_type()) {
  //         KNM_ref(i_row, i_col) = 0;
  //       }
  //       ++i_col;
  //     }
  //     ++i_row;
  //   }
  // }


  // json kernel_hypers{
  //       {"zeta", 1}, {"target_type", "Atom"}, {"name", "GAP"}};
  // SparseKernel kernel{kernel_hypers};

  // auto KNM_test{kernel.compute(representation, managers, sparse_points)};

  // auto diff = math::relative_error(KNM_ref, KNM_test);

  // std::cout << diff << std::endl;
  // std::cout << "============================" << std::endl;
  // std::cout << KNM_ref<< std::endl;
  // std::cout << "============================" << std::endl;
  // std::cout << KNM_test<< std::endl;
  // sparse_points.push_back(representation, managers, selected_ids);

  // auto feat_ref = managers.get_features(representation);

  // auto feat_test = sparse_points.get_features();
  // for (int i_row{0}; i_row < feat_ref.rows(); i_row++) {
  //   auto diff = (feat_ref.rowwise() - feat_test.row(i_row)).rowwise().lpNorm<1>();
  //   std::cout << "Number of matching row "<< i_row << " :" << (diff.array() < 1e-16).count() << std::endl;
  // }

  // std::cout << feat_ref<< std::endl;
  // std::cout << "============================" << std::endl;
  // std::cout << feat_test<< std::endl;
  // for (auto center : manager) {
  //   // if (center_count >= n_centers_print) {
  //   //   break;
  //   // }
  //   size_t n_species_center{soap_vectors.get_keys(center).size()};
  //   std::cout << "============================" << std::endl;
  //   std::cout << "Center " << center.get_index();
  //   std::cout << " of type " << center.get_atom_type() << std::endl;
  //   int maxRow, maxCol;
  //   auto diff_rep_m{math::relative_error(soap_vectors.get_dense_row(center), soap_vectors_half.get_dense_row(center), 1e-15)};
  //   double diff_rep = diff_rep_m.maxCoeff(&maxRow, &maxCol);
  //   std::cout << "max error: " << diff_rep << " ref val: " << soap_vectors.get_dense_row(center)(maxRow, maxCol) << " Nb_neigh: "<< center.pairs().size() << std::endl;
  //   if (diff_rep > 1e-13) {
  //     std::cout << "Ref: " << std::endl<< soap_vectors.get_dense_row(center);
  //     std::cout << std::endl;
  //     std::cout << "Test: " << std::endl<< soap_vectors_half.get_dense_row(center);
  //     std::cout << std::endl;
  //   }
  //   auto keys_center = soap_vectors[center].get_keys();
  //   std::cout << "Center data keys: ";
  //   for (auto key : keys_center) {
  //     std::cout << "(";
  //     for (auto key_sp : key) {
  //       std::cout << key_sp << ", ";
  //     }
  //     std::cout << "\b\b) ";
  //   }
  //   std::cout << std::endl;
  //   auto ii_pair = center.get_atom_ii();

  //   auto half_it = manager_half->get_iterator_at(center_count, 0);
  //   auto half_center = *(half_it);
  //   std::cout << "Tags:  (";
  //   for (auto neigh : half_center.pairs()) {
  //     auto neigh_type = neigh.get_atom_tag();
  //     std::cout << neigh_type << ", ";
  //   }
  //   std::cout << "\b\b) ";
  //   std::cout << std::endl;

  //   std::cout << "Types: (";
  //   for (auto neigh : half_center.pairs()) {
  //     auto neigh_type = neigh.get_atom_type();
  //     std::cout << neigh_type << ", ";
  //   }
  //   std::cout << "\b\b) ";
  //   std::cout << std::endl;

  //   auto diff_ii_m{math::relative_error(soap_vector_gradients.get_dense_row(ii_pair), soap_vector_gradients_half.get_dense_row(half_center.get_atom_ii()), 1e-15)};
  //   double diff_ii = diff_ii_m.maxCoeff(&maxRow, &maxCol);
  //   std::cout << "max error: " << diff_ii << " ref val: " << soap_vector_gradients.get_dense_row(ii_pair)(maxRow, maxCol) << " Nb_neigh: "<< center.pairs().size() << std::endl;
  //   if (diff_ii > 1e-12) {
  //     std::cout << "Ref: " << std::endl<< soap_vector_gradients.get_dense_row(ii_pair).transpose();
  //     std::cout << std::endl;
  //     std::cout << "Test: " << std::endl<< soap_vector_gradients_half.get_dense_row(half_center.get_atom_ii()).transpose();
  //     std::cout << std::endl;
  //   }
  //   auto keys_grad_center = soap_vector_gradients[ii_pair].get_keys();
  //   std::cout << "Center gradient keys: ";
  //   for (auto key : keys_grad_center) {
  //     std::cout << "(";
  //     for (auto key_sp : key) {
  //       std::cout << key_sp << ", ";
  //     }
  //     std::cout << "\b\b) ";
  //   }
  //   std::cout << std::endl;

  //   size_t neigh_count{0};
  //   for (auto neigh : center.pairs()) {
  //     // if (neigh_count >= n_neigh_print) {
  //     //   break;
  //     // }
  //     auto neigh_type = neigh.get_atom_type();
  //     auto tags = neigh.get_atom_tag_list();
  //     if (tags[1] <= tags[0]) {continue;}
  //     auto half_neigh_it = half_center.template get_clusters_of_order<2>(1+neigh_count).begin();
  //     // auto half_neigh_it = half_center.pairs().begin();
  //     // for (size_t ii{0}; ii < neigh_count; ii++) {
  //     //   ++half_neigh_it;
  //     // }
  //     auto half_neigh = *(half_neigh_it);

  //     std::cout << "Neighbour "<< neigh_type <<" tags: ";

  //     std::cout << "(";
  //     for (auto tag : tags) {
  //       std::cout << tag << ", ";
  //     }
  //     std::cout << "\b\b) ";
  //     std::cout << std::endl;

  //     auto keys_neigh = soap_vector_gradients[neigh].get_keys();
  //     std::cout << "Neighbour "<< neigh_type <<" keys: ";
  //     for (auto key : keys_neigh) {
  //       std::cout << "(";
  //       for (auto key_sp : key) {
  //         std::cout << key_sp << ", ";
  //       }
  //       std::cout << "\b\b) ";
  //     }
  //     std::cout << std::endl;
  //     auto keys_neigh_half = soap_vector_gradients_half[half_neigh].get_keys();
  //     std::cout << " /// ";
  //     for (auto key : keys_neigh_half) {
  //       std::cout << "(";
  //       for (auto key_sp : key) {
  //         std::cout << key_sp << ", ";
  //       }
  //       std::cout << "\b\b) ";
  //     }
  //     std::cout << std::endl;
  //     auto diff_ij_m{math::relative_error(soap_vector_gradients.get_dense_row(neigh), soap_vector_gradients_half.get_dense_row(half_neigh), 1e-10)};
  //   double diff_ij = diff_ij_m.maxCoeff(&maxRow, &maxCol);
  //   std::cout << "max error: " << diff_ij << " ref val: " << soap_vector_gradients.get_dense_row(neigh)(maxRow, maxCol) << " Nb_neigh: "<< center.pairs().size() << std::endl;
  //     if (diff_ij > 1e-12) {
  //       std::cout << "Ref: " << std::endl<< soap_vector_gradients.get_dense_row(neigh).transpose();
  //       std::cout << std::endl;
  //       std::cout << "Test: " << std::endl<< soap_vector_gradients_half.get_dense_row(half_neigh).transpose();
  //       std::cout << std::endl;
  //     }

  //     ++neigh_count;
  //   }
  //   ++center_count;
  // }
}
