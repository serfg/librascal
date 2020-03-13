/**
 * @file   rascal/models/pseudo_points.hh
 *
 * @author Felix Musil <felix.musil@epfl.ch>
 *
 * @date   18 Dec 2019
 *
 * @brief Implementation of pseudo points for sparse kernels
 *
 * Copyright 2019 Felix Musil COSMO (EPFL), LAMMM (EPFL)
 *
 * Rascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * Rascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software; see the file LICENSE. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef SRC_RASCAL_MODELS_PSEUDO_POINTS_HH_
#define SRC_RASCAL_MODELS_PSEUDO_POINTS_HH_

#include "rascal/math/utils.hh"
#include "rascal/representations/calculator_base.hh"
#include "rascal/structure_managers/property_block_sparse.hh"
#include "rascal/structure_managers/structure_manager.hh"
#include "rascal/structure_managers/structure_manager_collection.hh"
#include "rascal/utils/json_io.hh"

namespace rascal {

  template <class Calculator>
  class PseudoPointsBlockSparse {
   public:
    using Matrix_t = math::Matrix_t;
    using MatrixMap_Ref_t = Eigen::Map<Matrix_t>;
    using MatrixMapConst_Ref_t = const Eigen::Map<const Matrix_t>;
    using MatrixRefConst_t = const Eigen::Ref<const Matrix_t>;
    using Key_t = std::vector<int>;
    using Keys_t = std::set<Key_t>;
    using SortedKey_t = internal::SortedKey<Key_t>;
    using InputData_t = internal::InternallySortedKeyMap<Key_t, Matrix_t>;
    using Data_t = Eigen::Array<double, Eigen::Dynamic, 1>;
    using Maps_t = std::vector<InputData_t>;

    template <class StructureManager>
    using Property_t =
        typename Calculator::template Property_t<StructureManager>;
    template <class StructureManager>
    using PropertyGradient_t =
        typename Calculator::template PropertyGradient_t<StructureManager>;

    Data_t values{};
    Maps_t maps{};

    size_t inner_size{0};
    //! list of possible center species for accessing [sp]
    std::set<int> center_species{};
    //! list of possible keys for accessing [key]
    std::set<Key_t> keys{};

    std::vector<int> sparse_points_sp{};

    std::map<int, Data_t> masks_map{};

    bool operator==(const PseudoPointsBlockSparse<Calculator> & other) const {
      if ((values == other.values).all()  and  // NOLINT
          (inner_size == other.inner_size) and                         // NOLINT
          (center_species == other.center_species) and                 // NOLINT
          (keys == other.keys)) {                                      // NOLINT
        return true;
      } else {
        return false;
      }
    }

    /**
     * Adjust size of values (only increases, never frees) and maps with the
     * same keys for each entries (order == 1 -> centers,
     * order == 2 -> neighbors ...).
     * The use of std::set is to garrantee the order of the keys.
     *
     * Args template parameters are there to accomodate with the
     * fact that most stl container that can be indexed have several optional
     * template parameters that need to be deduced.
     * The first template argument of Args is expected to be of type Key_t or
     * SortedKey<Key_t>.
     */
    template <typename... Args>
    void resize(const std::set<Args...> & keys, const int& n_sparse_points, const int& inner_size) {
      this->maps.resize(n_sparse_points, std::move(InputData_t(this->values)));
      size_t global_offset{0};
      for (size_t i_map{0}; i_map < this->size(); i_map++) {
        this->maps[i_map].resize_view(keys, 1, inner_size, global_offset);
        global_offset += this->maps[i_map].size();
      }
      this->values.resize(global_offset);
      this->setZero();
    }

    void setZero() { this->values = 0.; }

    size_t size() const { return this->maps.size(); }

    /**
     * @return set of unique keys at the level of the structure
     */
    const Keys_t& get_keys() const {
      return keys;
    }

    /**
     * Get a dense feature matrix Ncenter x Nfeatures only if
     * `this->are_keys_uniform == true`. It only reinterprets the flat data
     * storage object since all cluster entries have the same number of keys,
     * i.e. each rows has the same size and has the same key ordering.
     */
    MatrixMapConst_Ref_t get_raw_data_view() const {
      auto && n_row = this->size();
      auto && n_col = this->values.size() / this->size();
      return MatrixMapConst_Ref_t(this->values.data(), n_row, n_col);
    }

    std::array<int, 4> get_block_info_by_key(const Key_t & key) const {
      SortedKey_t skey{key};
      return this->get_block_info_by_key(skey);
    }

    std::array<int, 4> get_block_info_by_key(const SortedKey_t & skey) const {
      auto && n_row = static_cast<int>(this->size());
      auto && n_col = static_cast<int>(this->inner_size);
      // since the keys are uniform we can use the first element of the map
      auto && view_start = this->maps[0].get_location_by_key(skey);
      // the block does all rows and the columns corresponding to skey
      std::array<int, 4> arr = {{0, view_start, n_row, n_col}};
      return arr;
    }

    //! Accessor for property by index for dynamically sized properties
    const Eigen::Block<const MatrixMapConst_Ref_t> operator[](Key_t key) const {
      const auto matA = this->get_raw_data_view();
      const auto mA_info = this->get_block_info_by_key(key);
      return matA.block(mA_info[0], mA_info[1], mA_info[2], mA_info[3]);
    }

    /**
     * Fill the pseudo points container with features computed with calculator
     * on the atomic structure contained in collection using
     * selected_center_indices to select which center to copy into the
     * container.
     *
     * selected_center_indices is a list of list of center indices relative to
     * the atomic structures.
     */
    template <class ManagerCollection>
    void
    push_back(const Calculator & calculator,
              const ManagerCollection & collection,
              const std::vector<std::vector<int>> & selected_center_indices) {
      using Manager_t = typename ManagerCollection::Manager_t;

      int n_sparse_points{0};

      for (size_t i_manager{0}; i_manager < collection.size(); ++i_manager) {
        const auto& manager = collection[i_manager];
        const auto& indices = selected_center_indices[i_manager];
        auto property = manager->template get_property<Property_t<Manager_t>>(
          calculator.get_name());
        for (const auto& idx : indices) {
          auto center_it = manager->get_iterator_at(idx);
          auto center = *center_it;
          auto keys_center = property->get_keys(center);
          this->keys.insert(keys_center.begin(), keys_center.end());
          this->sparse_points_sp.push_back(center.get_atom_type());
          this->center_species.insert(center.get_atom_type());
          ++n_sparse_points;
        }
        this->inner_size = property->get_nb_comp();
      }
      this->resize(this->keys, n_sparse_points, this->inner_size);
      for (const auto& sp : this->center_species) {
        this->masks_map[sp].resize(n_sparse_points);
        this->masks_map[sp] = 0.;
      }

      for (size_t i_sparse{0}; i_sparse < this->sparse_points_sp.size(); ++i_sparse) {
        this->masks_map[this->sparse_points_sp[i_sparse]][i_sparse] = 1.;
      }

      n_sparse_points = 0;
      for (size_t i_manager{0}; i_manager < collection.size(); ++i_manager) {
        const auto& manager = collection[i_manager];
        const auto& indices = selected_center_indices[i_manager];
        auto property =  manager->template get_property<Property_t<Manager_t>>(
            calculator.get_name());
        for (const auto& idx : indices) {
          auto center_it = manager->get_iterator_at(idx);
          auto center = *center_it;
          auto && prop_row = property->operator[](center);
          for (const auto& key : prop_row.get_keys()) {
            this->maps[n_sparse_points][key] = prop_row.flat(key);
          }
          ++n_sparse_points;
        }
      }
    }

    //! dot product with itself to build the K_{MM} kernel matrix
    math::Matrix_t self_dot() const {
      math::Matrix_t KMM{this->size(), this->size()};
      KMM.setZero();
      for (const Key_t & key : keys) {
        KMM += this->operator[](key) * this->operator[](key).transpose();
      }
      for (int i_sparse_pt{0}; i_sparse_pt < KMM.rows(); ++i_sparse_pt) {
        const int& sp{this->sparse_points_sp[i_sparse_pt]};
        const auto& mask = this->masks_map.at(sp);
        KMM.array().row(i_sparse_pt) *= mask.transpose();
        KMM.array().col(i_sparse_pt) *= mask;
      }
      return KMM;
    }

    using ColVector_t = Eigen::Matrix<double, Eigen::Dynamic, 1>;

    /**
     * Compute the dot product between the pseudo points associated with type
     * sp with the representation associated with a center.
     * @return column vector Mx1
     */
    template <class Val>
    ColVector_t
    dot(const int & sp,
        internal::InternallySortedKeyMap<Key_t, Val> & representation) const {
      ColVector_t KNM_row(this->size());
      KNM_row.setZero();
      if (this->center_species.count(sp) == 0) {
        // the type of the central atom is not in the pseudo points
        return KNM_row;
      }

      for (const Key_t & key : this->keys) {
        if (representation.count(key)) {
          auto rep_flat_by_key{representation.flat(key)};
          KNM_row += this->operator[](key) * rep_flat_by_key.transpose();
        }
      }

      const auto& mask = this->masks_map.at(sp);
      KNM_row.array() *= mask;

      return KNM_row;
    }

    using ColVectorDer_t =
        Eigen::Matrix<double, Eigen::Dynamic, ThreeD, Eigen::ColMajor>;
    /**
     * Compute the dot product between the pseudo points associated with type
     * sp with the gradient of the representation associated with a center.
     * @return column vector Mx3
     */
    template <class Val>
    ColVectorDer_t dot_derivative(const int & sp,
                                  internal::InternallySortedKeyMap<Key_t, Val> &
                                      representation_grad) const {
      // const auto & values_by_sp = this->values.at(sp);
      // const auto & indices_by_sp = this->indices.at(sp);
      ColVectorDer_t KNM_row(this->size(), ThreeD);
      KNM_row.setZero();
      if (this->center_species.count(sp) == 0) {
        // the type of the central atom is not in the pseudo points
        return KNM_row;
      }

      for (const Key_t & key : this->keys) {
        if (representation_grad.count(key)) {
          // get the representation gradient features and shape it
          // assumes the gradient directions are the outermost index
          auto rep_grad_flat_by_key{representation_grad.flat(key)};
          Eigen::Map<const Eigen::Matrix<double, ThreeD, Eigen::Dynamic,
                                         Eigen::RowMajor>>
              rep_grad_by_key(rep_grad_flat_by_key.data(), ThreeD,
                              this->inner_size);
          assert(rep_grad_flat_by_key.size() ==
                 static_cast<int>(ThreeD * this->inner_size));


          // compute the product between pseudo points and representation
          // gradient block
          KNM_row += this->operator[](key) * rep_grad_by_key.transpose();
        }      // if
      }        // key
      return KNM_row;
    }

    math::Matrix_t get_features() const {
      math::Matrix_t mat{this->size(), this->inner_size * this->keys.size()};
      mat = this->get_raw_data_view();
      return mat;
    }

  };

  // /**
  //  * Set of pseudo points associated with a representation stored in BlockSparse
  //  * format, e.g. SphericalInvariants. The number of pseudo points is often
  //  * referred as $M$ and note that they might be the representation of actual
  //  * atomic environments or completely artificial.
  //  *
  //  * Pseudo points are useful to build sparse kernel models such as Subset of
  //  * Regressors. This class is tailored for building property models that
  //  * depend on the type of the central atom.
  //  */
  // template <class Calculator>
  // class PseudoPointsBlockSparse {
  //  public:
  //   using Key_t = typename CalculatorBase::Key_t;
  //   using Data_t = std::map<int, std::map<Key_t, std::vector<double>>>;
  //   using Indices_t = std::map<int, std::map<Key_t, std::vector<size_t>>>;
  //   using Counters_t = std::map<int, size_t>;

  //   template <class StructureManager>
  //   using Property_t =
  //       typename Calculator::template Property_t<StructureManager>;
  //   template <class StructureManager>
  //   using PropertyGradient_t =
  //       typename Calculator::template PropertyGradient_t<StructureManager>;

  //   /**
  //    * Container for the actual data. By construction it gathers pseudo points
  //    * in a particular manner: [sp][key][feature_idx] ; where sp is the type
  //    * of the central atom to which these features correspond to, key is a
  //    * valid key taken from the BlockSparse original container and feature_idx
  //    * is a linear index containing all the features associated to several
  //    * sparse points.
  //    * This storage keeps the minimal amount of data.
  //    */
  //   Data_t values{};
  //   /**
  //    * Container for the sparse points indices arranged like:
  //    * [sp][key][sparse_point_idx] ; where sp and key are like in values and
  //    * sparse_point_idx are the indices of the sparse points in the particular
  //    * [sp][key] bin.
  //    */
  //   Indices_t indices{};
  //   //! counts number of sparse points for each [sp]
  //   Counters_t counters{};
  //   //! size of one feature block in [sp][key]
  //   size_t inner_size{0};
  //   //! list of possible center species for accessing [sp]
  //   std::set<int> center_species{};
  //   //! list of possible keys for accessing [key]
  //   std::set<Key_t> keys{};

  //   PseudoPointsBlockSparse() {
  //     // there is less than 130 elemtents
  //     for (int sp{1}; sp < 130; ++sp) {
  //       counters[sp] = 0;
  //     }
  //   }

  //   bool operator==(const PseudoPointsBlockSparse<Calculator> & other) const {
  //     if ((values == other.values) and (indices == other.indices) and  // NOLINT
  //         (counters == other.counters) and                             // NOLINT
  //         (inner_size == other.inner_size) and                         // NOLINT
  //         (center_species == other.center_species) and                 // NOLINT
  //         (keys == other.keys)) {                                      // NOLINT
  //       return true;
  //     } else {
  //       return false;
  //     }
  //   }

  //   //! get number of sparse points in the container
  //   size_t size() const {
  //     size_t n_points{0};
  //     for (const auto & count : this->counters) {
  //       n_points += count.second;
  //     }
  //     return n_points;
  //   }
  //   //! get number of sparse points for a given central atom type
  //   size_t size_by_species(const int & sp) const {
  //     return this->counters.at(sp);
  //   }
  //   //! get the registered central atom species
  //   const std::set<int> & species() const { return center_species; }

  //   //! get array of central atom species for each pseudo point
  //   std::vector<int> species_by_points() const {
  //     std::vector<int> species{};
  //     for (const auto & sp : this->center_species) {
  //       for (size_t ii{0}; ii < counters.at(sp); ++ii) {
  //         species.push_back(sp);
  //       }
  //     }
  //     return species;
  //   }
  //   //! dot product with itself to build the K_{MM} kernel matrix
  //   math::Matrix_t self_dot(const int & sp) const {
  //     math::Matrix_t KMM_by_sp(this->counters.at(sp), this->counters.at(sp));
  //     KMM_by_sp.setZero();
  //     const auto & values_by_sp = values.at(sp);
  //     const auto & indices_by_sp = indices.at(sp);
  //     for (const Key_t & key : keys) {
  //       const auto & indices_by_sp_key = indices_by_sp.at(key);
  //       auto mat = Eigen::Map<const math::Matrix_t>(
  //           values_by_sp.at(key).data(),
  //           static_cast<Eigen::Index>(indices_by_sp_key.size()),
  //           static_cast<Eigen::Index>(this->inner_size));
  //       // the following does the same as:
  //       // auto KMM_by_key = (mat * mat.transpose()).eval();
  //       math::Matrix_t KMM_by_key(indices_by_sp_key.size(),
  //                                 indices_by_sp_key.size());
  //       KMM_by_key =
  //           KMM_by_key.setZero().selfadjointView<Eigen::Upper>().rankUpdate(
  //               mat);
  //       for (int i_row{0}; i_row < KMM_by_key.rows(); i_row++) {
  //         for (int i_col{0}; i_col < KMM_by_key.cols(); i_col++) {
  //           KMM_by_sp(indices_by_sp_key[i_row], indices_by_sp_key[i_col]) +=
  //               KMM_by_key(i_row, i_col);
  //         }
  //       }
  //     }  // key
  //     return KMM_by_sp;
  //   }

  //   using ColVector_t = Eigen::Matrix<double, Eigen::Dynamic, 1>;

  //   /**
  //    * Compute the dot product between the pseudo points associated with type
  //    * sp with the representation associated with a center.
  //    * @return column vector Mx1
  //    */
  //   template <class Val>
  //   ColVector_t
  //   dot(const int & sp,
  //       internal::InternallySortedKeyMap<Key_t, Val> & representation) const {
  //     const auto & values_by_sp = this->values.at(sp);
  //     const auto & indices_by_sp = this->indices.at(sp);
  //     ColVector_t KNM_row(this->size());
  //     KNM_row.setZero();
  //     if (this->center_species.count(sp) == 0) {
  //       // the type of the central atom is not in the pseudo points
  //       return KNM_row;
  //     }
  //     int offset{0};
  //     for (const int & csp : this->center_species) {
  //       if (csp == sp) {
  //         break;
  //       } else {
  //         offset += this->counters.at(csp);
  //       }
  //     }

  //     for (const Key_t & key : this->keys) {
  //       if (representation.count(key)) {
  //         auto rep_flat_by_key{representation.flat(key)};
  //         const auto & indices_by_sp_key = indices_by_sp.at(key);
  //         auto mat = Eigen::Map<const math::Matrix_t>(
  //             values_by_sp.at(key).data(),
  //             static_cast<Eigen::Index>(indices_by_sp_key.size()),
  //             static_cast<Eigen::Index>(this->inner_size));
  //         auto KNM_row_key = (mat * rep_flat_by_key.transpose()).eval();
  //         for (int i_row{0}; i_row < KNM_row_key.size(); i_row++) {
  //           KNM_row(offset + indices_by_sp_key[i_row]) += KNM_row_key(i_row);
  //         }
  //       }
  //     }
  //     return KNM_row;
  //   }

  //   using ColVectorDer_t =
  //       Eigen::Matrix<double, Eigen::Dynamic, ThreeD, Eigen::ColMajor>;
  //   /**
  //    * Compute the dot product between the pseudo points associated with type
  //    * sp with the gradient of the representation associated with a center.
  //    * @return column vector Mx3
  //    */
  //   template <class Val>
  //   ColVectorDer_t dot_derivative(const int & sp,
  //                                 internal::InternallySortedKeyMap<Key_t, Val> &
  //                                     representation_grad) const {
  //     const auto & values_by_sp = this->values.at(sp);
  //     const auto & indices_by_sp = this->indices.at(sp);
  //     ColVectorDer_t KNM_row(this->size(), ThreeD);
  //     KNM_row.setZero();
  //     if (this->center_species.count(sp) == 0) {
  //       // the type of the central atom is not in the pseudo points
  //       return KNM_row;
  //     }
  //     int offset{0};
  //     for (const int & csp : this->center_species) {
  //       if (csp == sp) {
  //         break;
  //       } else {
  //         offset += this->counters.at(csp);
  //       }
  //     }

  //     for (const Key_t & key : this->keys) {
  //       if (representation_grad.count(key)) {
  //         // get the representation gradient features and shape it
  //         // assumes the gradient directions are the outermost index
  //         auto rep_grad_flat_by_key{representation_grad.flat(key)};
  //         Eigen::Map<const Eigen::Matrix<double, ThreeD, Eigen::Dynamic,
  //                                        Eigen::RowMajor>>
  //             rep_grad_by_key(rep_grad_flat_by_key.data(), ThreeD,
  //                             this->inner_size);
  //         assert(rep_grad_flat_by_key.size() ==
  //                static_cast<int>(ThreeD * this->inner_size));
  //         const auto & indices_by_sp_key = indices_by_sp.at(key);
  //         // get the block of pseudo points features
  //         auto mat = Eigen::Map<const math::Matrix_t>(
  //             values_by_sp.at(key).data(),
  //             static_cast<Eigen::Index>(indices_by_sp_key.size()),
  //             static_cast<Eigen::Index>(this->inner_size));
  //         assert(indices_by_sp_key.size() * this->inner_size ==
  //                values_by_sp.at(key).size());
  //         // compute the product between pseudo points and representation
  //         // gradient block
  //         // ColVectorDer_t KNM_row_key(indices_by_sp_key.size(), ThreeD);
  //         ColVectorDer_t KNM_row_key = mat * rep_grad_by_key.transpose();
  //         // dispatach kernel partial elements to the proper pseudo points
  //         // indices
  //         // for (int i_dim{0}; i_dim < ThreeD; i_dim++) {
  //         //   for (int i_row{0}; i_row < KNM_row_key.rows(); i_row++) {
  //         //     KNM_row(offset + indices_by_sp_key[i_row], i_dim) +=
  //         //         KNM_row_key(i_row, i_dim);
  //         //   }  // M
  //         // }    // dim
  //         // for (int i_dim{0}; i_dim < ThreeD; i_dim++) {
  //         for (int i_row{0}; i_row < KNM_row_key.rows(); i_row++) {
  //           KNM_row.row(offset + indices_by_sp_key[i_row]) +=
  //               KNM_row_key.row(i_row);
  //         }  // M
  //         // }    // dim
  //       }      // if
  //     }        // key
  //     return KNM_row;
  //   }

  //   /**
  //    * Fill the pseudo points container with features computed with calculator
  //    * on the atomic structure contained in collection using
  //    * selected_center_indices to select which center to copy into the
  //    * container.
  //    *
  //    * selected_center_indices is a list of list of center indices relative to
  //    * the atomic structures.
  //    */
  //   template <class ManagerCollection>
  //   void
  //   push_back(const Calculator & calculator,
  //             const ManagerCollection & collection,
  //             const std::vector<std::vector<int>> & selected_center_indices) {
  //     for (size_t i_manager{0}; i_manager < collection.size(); ++i_manager) {
  //       this->push_back(calculator, collection[i_manager],
  //                       selected_center_indices[i_manager]);
  //     }
  //   }

  //   template <class StructureManager>
  //   void push_back(const Calculator & calculator,
  //                  std::shared_ptr<StructureManager> manager,
  //                  const std::vector<int> & selected_center_indices) {
  //     std::string prop_name{calculator.get_name()};
  //     auto property =
  //         manager->template get_property<Property_t<StructureManager>>(
  //             prop_name);
  //     for (const auto & center_index : selected_center_indices) {
  //       auto center_it = manager->get_iterator_at(center_index);
  //       auto center = *center_it;
  //       auto && map2row = property->operator[](center);
  //       this->push_back(map2row, center.get_atom_type());
  //     }
  //   }

  //   template <class Val>
  //   void push_back(internal::InternallySortedKeyMap<Key_t, Val> & pseudo_point,
  //                  const int & center_type) {
  //     auto & values_by_sp = this->values[center_type];
  //     auto & counters_by_sp = this->counters[center_type];
  //     auto & indices_by_sp = this->indices[center_type];
  //     this->center_species.insert(center_type);
  //     for (const auto & key : pseudo_point.get_keys()) {
  //       this->keys.insert(key);
  //       auto & values_by_sp_key = values_by_sp[key];
  //       auto pseudo_point_by_key = pseudo_point.flat(key);
  //       for (int ii{0}; ii < pseudo_point_by_key.size(); ++ii) {
  //         values_by_sp_key.push_back(pseudo_point_by_key[ii]);
  //       }
  //       indices_by_sp[key].push_back(counters_by_sp);
  //       if (this->inner_size == 0) {
  //         this->inner_size = pseudo_point_by_key.size();
  //       } else if (static_cast<Eigen::Index>(this->inner_size) !=
  //                  pseudo_point_by_key.size()) {
  //         std::stringstream err_str{};
  //         err_str << "The representation changed size during the set-up of "
  //                    "PseudoPointsBlockSparse:"
  //                 << "'" << this->inner_size
  //                 << "!=" << pseudo_point_by_key.size() << "'.";
  //         throw std::logic_error(err_str.str());
  //       }
  //     }
  //     ++counters_by_sp;
  //   }

  //   math::Matrix_t get_features() const {
  //     size_t n_pseudo_points{0};
  //     for (const auto & sp : this->center_species) {
  //       n_pseudo_points += counters.at(sp);
  //     }
  //     math::Matrix_t mat{n_pseudo_points, this->inner_size * this->keys.size()};
  //     mat.setZero();
  //     size_t i_row{0};
  //     for (const auto & sp : this->center_species) {
  //       auto & values_by_sp = this->values.at(sp);
  //       auto & indices_by_sp = this->indices.at(sp);
  //       size_t i_col{0};
  //       for (const auto & key : this->keys) {
  //         if (values_by_sp.count(key)) {
  //           Eigen::Map<const math::Matrix_t> block{
  //               values_by_sp.at(key).data(),
  //               static_cast<Eigen::Index>(indices_by_sp.at(key).size()),
  //               static_cast<Eigen::Index>(this->inner_size)};
  //           for (size_t ii{0}; ii < indices_by_sp.at(key).size(); ii++) {
  //             mat.block(i_row + indices_by_sp.at(key)[ii], i_col, 1,
  //                       this->inner_size) = block.row(ii);
  //           }
  //         }
  //         i_col += this->inner_size;
  //       }
  //       i_row += this->counters.at(sp);
  //     }
  //     return mat;
  //   }
  // };



}  // namespace rascal

#endif  // SRC_RASCAL_MODELS_PSEUDO_POINTS_HH_
