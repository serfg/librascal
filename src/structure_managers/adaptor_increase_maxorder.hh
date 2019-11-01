/**
 * @file   adaptor_increase_maxorder.hh
 *
 * @author Markus Stricker <markus.stricker@epfl.ch>
 *
 * @date   19 Jun 2018
 *
 * @brief implements an adaptor for structure_managers, which
 * creates a full and half neighbourlist if there is none and
 * triplets/quadruplets, etc. if existent.
 *
 * Copyright  2018 Markus Stricker, COSMO (EPFL), LAMMM (EPFL)
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

#ifndef SRC_STRUCTURE_MANAGERS_ADAPTOR_INCREASE_MAXORDER_HH_
#define SRC_STRUCTURE_MANAGERS_ADAPTOR_INCREASE_MAXORDER_HH_

#include "basic_types.hh"
#include "lattice.hh"
#include "rascal_utility.hh"
#include "structure_managers/property.hh"
#include "structure_managers/structure_manager.hh"

#include <set>
#include <vector>

namespace rascal {
  /**
   * Forward declaration for traits
   */
  template <class ManagerImplementation>
  class AdaptorMaxOrder;

  /**
   * Specialisation of traits for increase <code>MaxOrder</code> adaptor
   */
  template <class ManagerImplementation>
  struct StructureManager_traits<AdaptorMaxOrder<ManagerImplementation>> {
    using parent_traits = StructureManager_traits<ManagerImplementation>;
    constexpr static AdaptorTraits::Strict Strict{AdaptorTraits::Strict::no};
    constexpr static bool HasDistances{false};
    constexpr static bool HasDirectionVectors{
        parent_traits::HasDirectionVectors};
    constexpr static int Dim{parent_traits::Dim};
    constexpr static bool HasCenterPair{parent_traits::HasCenterPair};
    constexpr static int StackLevel{parent_traits::StackLevel + 1};
    // New MaxOrder upon construction
    constexpr static size_t MaxOrder{parent_traits::MaxOrder + 1};
    // TODO(felix) here we assume that only triplets can be added. need to
    // changed after the rework of AdaptorMaxOrder so that it passes the
    // Order that has been added.
    using AvailableOrdersList = typename internal::AppendAvailableOrder<3, typename parent_traits::AvailableOrdersList>::type;
    // Extend the layer by one with the new MaxOrder
    using LL = typename LayerIncreaser<parent_traits::MaxOrder,
                        typename parent_traits::LayerByOrder>::type;
    using LayerByOrder =
        typename LayerExtender<MaxOrder,
                               LL>::type;
  };

  /* ---------------------------------------------------------------------- */
  /**
   * Adaptor that increases the MaxOrder of an existing StructureManager. This
   * means, if the manager does not have a neighbourlist, there is nothing this
   * adaptor can do (hint: use adaptor_neighbour_list before and stack this on
   * top), if it exists, triplets, quadruplets, etc. lists are created.
   */
  template <class ManagerImplementation>
  class AdaptorMaxOrder
      : public StructureManager<AdaptorMaxOrder<ManagerImplementation>>,
        public std::enable_shared_from_this<
            AdaptorMaxOrder<ManagerImplementation>> {
   public:
    using Manager_t = AdaptorMaxOrder<ManagerImplementation>;
    using Parent = StructureManager<Manager_t>;
    using ManagerImplementation_t = ManagerImplementation;
    using ImplementationPtr_t = std::shared_ptr<ManagerImplementation>;
    using traits = StructureManager_traits<AdaptorMaxOrder>;
    using AtomRef_t = typename ManagerImplementation::AtomRef_t;
    template <size_t Order>
    using ClusterRef_t =
        typename ManagerImplementation::template ClusterRef<Order>;
    using Vector_ref = typename Parent::Vector_ref;
    using Hypers_t = typename Parent::Hypers_t;

    static constexpr size_t AdditionalOrder{internal::get_last_element_in_sequence(typename traits::AvailableOrdersList{})};

    constexpr static auto AtomLayer{
        Manager_t::template cluster_layer_from_order<1>()};
    constexpr static auto PairLayer{
        Manager_t::template cluster_layer_from_order<2>()};

    static_assert(traits::MaxOrder > 2,
                  "ManagerImplementation needs at least a pair list for"
                  " extension.");

    //! Default constructor
    AdaptorMaxOrder() = delete;

    /**
     * Given at least a pair list, this adaptor creates the next Order
     * list. I.e. from pairs to triplets, triplets to quadruplet, etc. Not
     * cutoff is needed, the cutoff is implicitly given by the neighbourlist,
     * which was built
     */
    explicit AdaptorMaxOrder(ImplementationPtr_t manager);

    AdaptorMaxOrder(ImplementationPtr_t manager, std::tuple<>)
        : AdaptorMaxOrder(manager) {}

    AdaptorMaxOrder(ImplementationPtr_t manager,
                    const Hypers_t & /*adaptor_hypers*/)
        : AdaptorMaxOrder(manager) {}

    //! Copy constructor
    AdaptorMaxOrder(const AdaptorMaxOrder & other) = delete;

    //! Move constructor
    AdaptorMaxOrder(AdaptorMaxOrder && other) = default;

    //! Destructor
    virtual ~AdaptorMaxOrder() = default;

    //! Copy assignment operator
    AdaptorMaxOrder & operator=(const AdaptorMaxOrder & other) = delete;

    //! Move assignment operator
    AdaptorMaxOrder & operator=(AdaptorMaxOrder && other) = default;

    /**
     * Updates just the adaptor assuming the underlying manager was
     * updated. this function invokes making triplets, quadruplets,
     * etc. depending on the MaxOrder, pair list has to be present.
     */
    void update_self();

    //! Updates the underlying manager as well as the adaptor
    template <class... Args>
    void update(Args &&... arguments);

    bool get_consider_ghost_neighbours() const { return true; }

    /**
     * Returns the linear indices of the clusters (whose atom tags are stored
     * in counters). For example when counters is just the list of atoms, it
     * returns the index of each atom. If counters is a list of pairs of indices
     * (i.e. specifying pairs), for each pair of indices i,j it returns the
     * number entries in the list of pairs before i,j appears.
     */
    template <size_t Order, bool T = (Order < 2), std::enable_if_t<T, int> = 0>
    size_t get_offset_impl(const std::array<size_t, Order> & counters) const {
      return this->manager->get_offset(counters);
    }

    template <size_t Order, bool T = (Order < 2), std::enable_if_t<not(T), int> = 0>
    size_t get_offset_impl(const std::array<size_t, Order> & counters) const {
      auto j{counters.back()};
      auto main_offset{this->offsets[j]};
      std::cout << "offset " << j << ", " << main_offset << std::endl;
      return main_offset;
    }


    //! Returns the number of clusters of size cluster_size
    size_t get_nb_clusters(size_t order) const {
      switch (order) {
      case traits::MaxOrder: {
        return this->neighbours_atom_tag.size();
        break;
      }
      default:
        return this->manager->get_nb_clusters(order);
        break;
      }
    }

    size_t get_size_with_ghosts() const {
      return this->manager->get_size_with_ghosts();
    }

    //! Returns number of clusters of the original manager
    size_t get_size() const { return this->manager->get_size(); }

    //! Returns position of an atom with index atom_tag
    Vector_ref get_position(size_t atom_tag) {
      return this->manager->get_position(atom_tag);
    }

    //! Returns position of the given atom object (useful for users)
    Vector_ref get_position(const AtomRef_t & atom) {
      return this->manager->get_position(atom.get_index());
    }

    //! get atom type from underlying manager
    int get_atom_type(int atom_tag) const {
      return this->manager->get_atom_type(atom_tag);
    }

    //! return atom type
    int get_atom_type(const AtomRef_t & atom) const {
      return this->manager->get_atom_type(atom.get_atom_tag());
    }

    /**
     * Returns the id of the index-th (neighbour) atom of the cluster that is
     * the full structure/atoms object, i.e. simply the id of the index-th atom
     */
    int get_neighbour_atom_tag(const Parent &, size_t index) const {
      return this->manager->get_neighbour_atom_tag(*this->manager, index);
    }

    //! Returns the id of the index-th neighbour atom of a given cluster
    // tag(felix) this should return an array
    template <size_t Layer>
    int get_neighbour_atom_tag(const ClusterRefKey<1, Layer> & cluster,
                               size_t index) const {
      return this->manager->get_neighbour_atom_tag(cluster, index);;
    }

    template <size_t Layer>
    std::array<int, AdditionalOrder - 1> get_neighbour_atom_tag_tt(const ClusterRefKey<1, Layer> & cluster,
                               size_t index) const {
      auto && offset = this->offsets[cluster.get_cluster_index(Layer)];
      return this->neighbours_atom_tag[offset + index];
    }



    size_t get_atom_index(const int atom_tag) const {
      return this->manager->get_atom_index(atom_tag);
    }

    //! Returns the number of neighbors of a given cluster
    template <size_t TargetOrder, size_t Order, size_t Layer, bool T = (TargetOrder < traits::MaxOrder - 1), std::enable_if_t<T, int> = 0>
    size_t
    get_cluster_size_impl(const ClusterRefKey<Order, Layer> & cluster) const {
      static_assert(TargetOrder < traits::MaxOrder,
                    "this implementation handles only the respective MaxOrder");
      /*
       * Here it is traits::MaxOrder-1, because only the current manager has the
       * right answer to the number of neighbours of the MaxOrder-1 tuple. This
       * is the 'else' case.
       */
      return this->manager->template get_cluster_size<TargetOrder>(cluster);
    }

    template <size_t TargetOrder, size_t Order, size_t Layer, bool T = (TargetOrder < traits::MaxOrder - 1), std::enable_if_t<not(T), int> = 0>
    size_t
    get_cluster_size_impl(const ClusterRefKey<Order, Layer> & cluster) const {
      static_assert(TargetOrder == traits::MaxOrder,
                    "this implementation handles only the respective MaxOrder");
      auto access_index = cluster.get_cluster_index(Layer);
      auto cluster_indices = cluster.get_cluster_indices();
      std::cout << std::endl;
      std::cout <<"cluster size "<< Layer << ", "<< access_index ;
      for (int ii{0}; ii < cluster_indices.size(); ii++) {
        std::cout << ", " << cluster_indices[ii];
      }
      std::cout  << std::endl;
      return this->nb_neigh[access_index];
    }

    //! Get the manager used to build the instance
    ImplementationPtr_t get_previous_manager() {
      return this->manager->get_shared_ptr();
    }

   protected:
    //! Extends the list containing the number of neighbours with a 0
    void add_entry_number_of_neighbours() { this->nb_neigh.push_back(0); }

    //! Adds a given atom tag as new cluster neighbour
    // tag(felix) this could have in principle more than one tag
    void add_neighbour_of_cluster(const std::array<int, AdditionalOrder - 1> atom_tag) {
    // void add_neighbour_of_cluster(const int atom_tag) {
      // adds `atom_tag` to neighbours
      this->neighbours_atom_tag.push_back(atom_tag);
      // increases the number of neighbours
      this->nb_neigh.back()++;
    }

    //! Sets the correct offsets for accessing neighbours
    void set_offsets() {
      auto n_tuples{nb_neigh.size()};
      if (n_tuples > 0) {
        this->offsets.emplace_back(0);
        for (size_t i{0}; i < n_tuples; ++i) {
          this->offsets.emplace_back(this->offsets[i] + this->nb_neigh[i]);
        }
      }
    }

    //! reference to underlying manager
    ImplementationPtr_t manager;

    //! Construct for reaching the MaxOrder and adding neighbours of at MaxOrder
    template <size_t Order, bool IsDummy>
    struct AddOrderLoop;

    //! Stores the number of neighbours for every traits::MaxOrder-1-clusters
    std::vector<size_t> nb_neigh{};

    //! Stores all neighbours atom tag of traits::MaxOrder-1-clusters
    std::vector< std::array<int, AdditionalOrder - 1> > neighbours_atom_tag{};
    // std::vector< int > neighbours_atom_tag{};

    /**
     * Stores the offsets of traits::MaxOrder-1-*clusters for accessing
     * `neighbours`, from where nb_neigh can be counted
     */
    std::vector<size_t> offsets{};

   private:
  };

  /* ---------------------------------------------------------------------- */
  //! Constructor of the next level manager
  template <class ManagerImplementation>
  AdaptorMaxOrder<ManagerImplementation>::AdaptorMaxOrder(
      std::shared_ptr<ManagerImplementation> manager)
      : manager{std::move(manager)}, nb_neigh{},
        neighbours_atom_tag{}, offsets{} {
    if (traits::MaxOrder < 3) {
      throw std::runtime_error("Increase MaxOrder: No pair list in underlying"
                               " manager.");
    }
  }

  /* ---------------------------------------------------------------------- */
  //! update, involing the update of the underlying manager
  template <class ManagerImplementation>
  template <class... Args>
  void AdaptorMaxOrder<ManagerImplementation>::update(Args &&... arguments) {
    this->manager->update(std::forward<Args>(arguments)...);
  }

  /* ---------------------------------------------------------------------- */
  //! structure for static looping up until pair order
  template <class ManagerImplementation>
  template <size_t Order, bool IsDummy>
  struct AdaptorMaxOrder<ManagerImplementation>::AddOrderLoop {
    static constexpr int OldMaxOrder{ManagerImplementation::traits::MaxOrder};
    using ClusterRef_t =
        typename ManagerImplementation::template ClusterRef<Order>;

    using NextOrderLoop = AddOrderLoop<Order + 1, (Order + 1 == OldMaxOrder)>;

    // do nothing, if MaxOrder is not reached, except call the next order
    static void loop(ClusterRef_t & cluster,
                     AdaptorMaxOrder<ManagerImplementation> & manager) {
      for (auto next_cluster : cluster.template get_clusters_of_order<Order+1>()) {
        auto & next_cluster_indices{std::get<next_cluster.order() - 1>(
            manager.cluster_indices_container)};

        // keep copying underlying cluster indices, they are not changed
        auto indices{next_cluster.get_cluster_indices()};
        next_cluster_indices.push_back(indices);

        NextOrderLoop::loop(next_cluster, manager);
      }
    }
  };

  /* ---------------------------------------------------------------------- */
  /**
   * At desired MaxOrder (plus one), here is where the magic happens and the
   * neighbours of the same order are added as the Order+1.  add check for non
   * half neighbour list.
   *
   * TODO: currently, this implementation is not distinguishing between minimal
   * and full lists. E.g. this has to be adjusted to include both, the i- and
   * the j-atoms of each pair as an i-atom in a triplet (center).
   */
  template <class ManagerImplementation>
  template <size_t Order>
  struct AdaptorMaxOrder<ManagerImplementation>::AddOrderLoop<Order, true> {
    static constexpr int OldMaxOrder{ManagerImplementation::traits::MaxOrder};

    using ClusterRef_t =
        typename ManagerImplementation::template ClusterRef<Order>;

    using traits = typename AdaptorMaxOrder<ManagerImplementation>::traits;

    //! loop through the orders to get to the maximum order, this is agnostic to
    //! the underlying MaxOrder, just goes to the maximum
    static void loop(ClusterRef_t & cluster,
                     AdaptorMaxOrder<ManagerImplementation> & manager) {
      // get all i_atoms to find neighbours to extend the cluster to the next
      // order
      auto i_atoms = cluster.get_atom_tag_list();

      // vector of existing i_atoms in `cluster` to avoid doubling of atoms in
      // final list
      std::vector<size_t> current_i_atoms{};

      // a set of new neighbours for the cluster, which will be added to extend
      // the cluster
      std::set<size_t> current_j_atoms{};

      // access to underlying manager for access to atom pairs
      auto & manager_tmp{cluster.get_manager()};

      // add an entry for the current clusters' neighbours
      manager.add_entry_number_of_neighbours();

      // careful: i_atoms can include ghosts: ghosts have to be ignored, since
      // they to not have a neighbour list themselves, they are only neighbours
      for (auto atom_tag : i_atoms) {
        current_i_atoms.push_back(atom_tag);
        size_t access_index = manager.get_neighbour_atom_tag(manager, atom_tag);

        // construct a shifted iterator to constuct a ClusterRef<1>
        auto iterator_at_position{manager_tmp.get_iterator_at(access_index)};

        // ClusterRef<1> as dereference from iterator to get pairs of the
        // i_atoms
        auto && j_cluster{*iterator_at_position};

        // collect all possible neighbours of the cluster: collection of all
        // neighbours of current_i_atoms
        for (auto pair : j_cluster.get_pairs()) {
          auto j_add = pair.back();
          if (j_add > i_atoms.back()) {
            current_j_atoms.insert(j_add);
          }
        }
      }



      // delete existing cluster atoms from list to build additional neighbours
      std::vector<size_t> atoms_to_add{};
      std::set_difference(current_j_atoms.begin(), current_j_atoms.end(),
                          current_i_atoms.begin(), current_i_atoms.end(),
                          std::inserter(atoms_to_add, atoms_to_add.begin()));

      // tag(felix) dummy tag for the j atom in the ijk triplet
      // only there waiting for the rework of this class
      if (atoms_to_add.size() > 0) {
        for (auto j : atoms_to_add) {
          manager.add_neighbour_of_cluster(std::array<int, 2>{0,static_cast<int>(j)});
        }
      }
    }
  };

  /* ---------------------------------------------------------------------- */
  /**
   * This is the loop, which runs recursively goes to the maximum Order and then
   * increases it by one (i.e. pairs->triplets, triplets->quadruplets, etc.
   */
  template <class ManagerImplementation>
  void AdaptorMaxOrder<ManagerImplementation>::update_self() {
    static_assert(traits::MaxOrder > 2,
                  "No neighbourlist present; extension not possible.");

    internal::for_each(this->cluster_indices_container,
                       internal::ResizePropertyToZero());

    auto & atom_cluster_indices{std::get<0>(this->cluster_indices_container)};
    auto & pair_cluster_indices{std::get<1>(this->cluster_indices_container)};
    this->nb_neigh.clear();
    this->offsets.clear();
    this->neighbours_atom_tag.clear();

    // #BUG8486@(markus) I now append the ghost atoms to the cluster index
    // container
    for (auto atom : this->manager) {
      Eigen::Matrix<size_t, AtomLayer + 1, 1> indices;
      indices.template head<AtomLayer>() = atom.get_cluster_indices();
      indices(AtomLayer) = indices(AtomLayer - 1);
      atom_cluster_indices.push_back(indices);

      std::vector<int> j_atom_tags{};
      for (auto pair : atom.get_pairs()) {
        j_atom_tags.push_back(pair.get_atom_tag());

        Eigen::Matrix<size_t, PairLayer + 1, 1> indices_pair;
        indices_pair.template head<PairLayer>() = pair.get_cluster_indices();
        indices_pair(PairLayer) = indices_pair(PairLayer - 1);;
        pair_cluster_indices.push_back(indices_pair);
      }

      this->add_entry_number_of_neighbours();
      for (auto j_atom_tag : j_atom_tags) {
        // add an entry for the current clusters' neighbours
        for (auto k_atom_tag : j_atom_tags) {
          if (j_atom_tag == k_atom_tag){ continue;}
          this->add_neighbour_of_cluster(std::array<int, 2>{j_atom_tag,k_atom_tag});
        }
      }


      //  Order 1, but variable Order is at 0, atoms, index 0
      // using AddOrderLoop =
      //     AddOrderLoop<atom.order(), atom.order() == (traits::MaxOrder - 1)>;

      // auto & atom_cluster_indices{std::get<0>(this->cluster_indices_container)};

      // auto indices = atom.get_cluster_indices();
      // atom_cluster_indices.push_back(indices);

      // AddOrderLoop::loop(atom, *this);

    }
    // correct the offsets for the new cluster order
    this->set_offsets();

    // atom_cluster_indices.fill_sequence();
    // pair_cluster_indices.fill_sequence();
    // add correct cluster_indices for the highest order
    auto & max_cluster_indices{
        std::get<traits::MaxOrder - 1>(this->cluster_indices_container)};
    max_cluster_indices.fill_sequence();
  }

  /* ---------------------------------------------------------------------- */



}  // namespace rascal

#endif  // SRC_STRUCTURE_MANAGERS_ADAPTOR_INCREASE_MAXORDER_HH_
