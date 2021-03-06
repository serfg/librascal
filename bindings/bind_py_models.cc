/**
 * @file   bind_py_models.cc
 *
 * @author Felix Musil <felix.musil@epfl.ch>
 *
 * @date   16 Jun 2019
 *
 * @brief  File for binding the models
 *
 * Copyright  2019  Felix Musil, COSMO (EPFL), LAMMM (EPFL)
 *
 * Rascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * Rascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Emacs; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "bind_py_models.hh"

namespace rascal {

  //! Register kernel classes
  template <class Kernel>
  static py::class_<Kernel> add_kernel(py::module & mod,
                                       py::module & /*m_internal*/) {
    std::string kernel_name = internal::GetBindingTypeName<Kernel>();
    py::class_<Kernel> kernel(mod, kernel_name.c_str());
    kernel.def(py::init([](const py::dict & hyper) {
      // convert to json
      json hypers = hyper;
      return std::make_unique<Kernel>(hypers);
    }));

    return kernel;
  }

  //! Register compute functions of the Kernel class
  template <internal::KernelType Type, class Calculator,
            class StructureManagers, class CalculatorBind>
  void bind_kernel_compute_function(CalculatorBind & kernel) {
    kernel.def("compute",
               py::overload_cast<const Calculator &, const StructureManagers &,
                                 const StructureManagers &>(
                   &Kernel::template compute<Calculator, StructureManagers>),
               py::call_guard<py::gil_scoped_release>(),
               R"(Compute the kernel between two sets of atomic structures,
              i.e. StructureManagerCollections. The representation of the
              atomic structures computed with calculator should have already
              been computed.)");
    kernel.def("compute",
               py::overload_cast<const Calculator &, const StructureManagers &>(
                   &Kernel::template compute<Calculator, StructureManagers>),
               py::call_guard<py::gil_scoped_release>(),
               R"(Compute the kernel between a set of atomic structures,
              i.e. StructureManagerCollections, and itself. The representation
              of the atomic structures computed with calculator should have
              already been computed.)");
  }

  //! Register compute functions of the SparseKernel class
  template <internal::SparseKernelType Type, class Calculator,
            class StructureManagers, class SparsePoints, class CalculatorBind>
  void bind_sparse_kernel_compute_function(CalculatorBind & kernel) {
    kernel.def(
        "compute",
        py::overload_cast<const Calculator &, const StructureManagers &,
                          const SparsePoints &>(
            &SparseKernel::template compute<Calculator, StructureManagers,
                                            SparsePoints>),
        py::call_guard<py::gil_scoped_release>(),
        R"(Compute the sparse kernel between the representation of a set of
            atomic structures, i.e. StructureManagerCollections, and a set of
            SparsePoints, i.e. the basis used by the sparse method.
            The representation of the atomic structures computed with Calculator
            should have already been computed.)");
    kernel.def("compute",
               py::overload_cast<const SparsePoints &>(
                   &SparseKernel::template compute<SparsePoints>),
               py::call_guard<py::gil_scoped_release>(),
               R"(Compute the kernel between a set of SparsePoints, i.e.
            the basis used by the sparse method, and itself.)");
    kernel.def(
        "compute_derivative",
        &SparseKernel::template compute_derivative<
            Calculator, StructureManagers, SparsePoints>,
        py::call_guard<py::gil_scoped_release>(),
        R"(Compute the sparse kernel between the gradient of representation of a
            set of atomic structures w.r.t. the atomic positions,
            i.e. StructureManagerCollections, and a set of SparsePoints, i.e.
            the basis used by the sparse method. The gradients of the
            representation of the atomic structures computed with Calculator
            should have already been computed.)");
  }

  //! Register a pseudo points class
  template <class SparsePoints>
  static py::class_<SparsePoints>
  add_sparse_points(py::module & mod, py::module & /*m_internal*/) {
    std::string pseudo_point_name =
        internal::GetBindingTypeName<SparsePoints>();
    py::class_<SparsePoints> sparse_points(mod, pseudo_point_name.c_str());
    sparse_points.def(py::init());
    sparse_points.def("size", &SparsePoints::size);
    sparse_points.def("get_features", &SparsePoints::get_features);
    return sparse_points;
  }

  //! register the population mechanism of the pseudo points class
  template <class ManagerCollection, class Calculator, class SparsePoints>
  void bind_sparse_points_push_back(py::class_<SparsePoints> & sparse_points) {
    using ManagerPtr_t = typename ManagerCollection::value_type;
    using Manager_t = typename ManagerPtr_t::element_type;
    sparse_points.def(
        "extend",
        py::overload_cast<const Calculator &, const ManagerCollection &,
                          const std::vector<std::vector<int>> &>(
            &SparsePoints::template push_back<ManagerCollection>),
        py::call_guard<py::gil_scoped_release>());
    sparse_points.def(
        "extend",
        py::overload_cast<const Calculator &, std::shared_ptr<Manager_t>,
                          const std::vector<int> &>(
            &SparsePoints::template push_back<Manager_t>),
        py::call_guard<py::gil_scoped_release>());
  }

  /**
   * Function to bind the representation managers to python
   *
   * @param mod pybind11 representation of the python module the represenation
   *             managers will be included to
   * @param m_internal pybind11 representation of the python module that are
   *                   needed but not useful to use on the python side
   *
   */
  void add_models(py::module & mod, py::module & m_internal) {
    // Defines a particular structure manager type
    using ManagerCollection_1_t =
        ManagerCollection<StructureManagerCenters, AdaptorNeighbourList,
                          AdaptorStrict>;
    using ManagerCollection_2_t =
        ManagerCollection<StructureManagerCenters, AdaptorNeighbourList,
                          AdaptorCenterContribution, AdaptorStrict>;
    // Defines the representation manager type for the particular structure
    // manager
    using Calc1_t = CalculatorSphericalInvariants;
    using SparsePoints_1_t = SparsePointsBlockSparse<Calc1_t>;

    // Bind the interface of this representation manager
    auto kernel = add_kernel<Kernel>(mod, m_internal);
    internal::bind_dict_representation(kernel);
    bind_kernel_compute_function<internal::KernelType::Cosine, Calc1_t,
                                 ManagerCollection_1_t>(kernel);
    bind_kernel_compute_function<internal::KernelType::Cosine, Calc1_t,
                                 ManagerCollection_2_t>(kernel);

    // bind the sparse kernel and pseudo points class
    auto sparse_kernel = add_kernel<SparseKernel>(mod, m_internal);
    internal::bind_dict_representation(sparse_kernel);
    bind_sparse_kernel_compute_function<internal::SparseKernelType::GAP,
                                        Calc1_t, ManagerCollection_2_t,
                                        SparsePoints_1_t>(sparse_kernel);
    auto sparse_points = add_sparse_points<SparsePoints_1_t>(mod, m_internal);
    bind_sparse_points_push_back<ManagerCollection_2_t, Calc1_t>(sparse_points);
    internal::bind_dict_representation(sparse_points);
  }
}  // namespace rascal
