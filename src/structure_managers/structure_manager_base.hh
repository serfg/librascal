/**
 * file   structure_manager_base.hh
 *
 * @author Till Junge <till.junge@epfl.ch>
 *
 * @date   06 Aug 2018
 *
 * @brief  Polymorphic base class for structure managers
 *
 * Copyright © 2018 Till Junge, COSMO (EPFL), LAMMM (EPFL)
 *
 * librascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * librascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Emacs; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef STRUCTURE_MANAGER_BASE_H
#define STRUCTURE_MANAGER_BASE_H

#include <string>

namespace rascal{

  /**
   * polymorphic base type for StructureManagers
   */

  class StructureManagerBase {
  public:
    inline decltype(auto) get_property(std::string name);
    virtual size_t nb_clusters(size_t cluster_size) const = 0;
  };



}  // rascal


#endif /* STRUCTURE_MANAGER_BASE_H */