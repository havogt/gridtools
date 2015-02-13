#ifndef _PROC_GRIDS_3D_H_
#define _PROC_GRIDS_3D_H_

#include <string>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <cmath>
#include "../GCL.h"
#include "../../common/array.h"
#include <boost/type_traits/integral_constant.hpp>

// This file needs to be changed

namespace gridtools {

  /** \class _3D_process_grid_t
   * Class that provides a generic 3D process grid from a linear
   * process distribution.  Given a contiguos range of P processes,
   * from 0 to P-1, this class provide a distribution of these
   * processes as a 3D grid

   1) by row in the best possible aspect ratio if GCL_3D_PROC_GRID environment variable is set to BLOCKED (default).

   2) by linear layout by I, J or K if GCL_3D_PROC_GRID environment variable is set BY_I, BY_J, or BY_K, resp.

   3) others to come

   * \n
   * \tparam CYCLIC is a boollist class \link boollist_concept \endlink template parameter that specifies which dimensions of the grid are clyclic
   * \n
   * This is a process grid matching the \ref proc_grid_concept concept
   */
  template <typename CYCLIC>
  struct _3D_process_grid_t {

    /** number of dimensions
     */
    static const int ndims = 3;

    typedef boost::false_type has_communicator;
    typedef CYCLIC period_type;
    period_type cyclic;
    int R, C,S;
    int r,c,s;

  private:
    int proc_idx(int rr, int cc, int ss) const {
      if (rr >= R || rr < 0
          || cc >= C || cc < 0
          || ss >= S || ss < 0)
        return -1;
      return rr*S*C+cc*S+ss;
    }

    void compute_id_coords(int pid) {
      int my_pid = pid;
      r = my_pid/(C*S);
      int t = pid - r* C*S;
      c = t/S;
      s = t-c*S;
    }

    void create_blocked(int P, int pid) {
      double obj, oldobj;
      //oldobj = sqrt((P-r3)*(P-r3)+(1-r3)*(1-r3)+(1-r3)*(1-r3));
      oldobj = 2*P+2+2*P; // Surface

      int sq = static_cast<int>(sqrt(static_cast<double>(P)));
      for (int i=1; i<=sq; ++i) {
        int t = P/i;
        if (i*t == P) {
          int sq1 = static_cast<int>(sqrt(static_cast<double>(t)));
          for (int j=1; j<=sq1; ++j) {
            int s = t/j;
            if (j*s == t) {
              // now i, j, s is a factorization of P
              //obj=sqrt((i-r3)*(i-r3)+(j-r3)*(j-r3)+(s-r3)*(s-r3));
              obj=2*i*j+2*i*s+2*j*s; //SURFACE
              if (obj <= oldobj) {
                oldobj=obj;
                R = s;
                S = j;
                C = i;
              }
            }
          }
        }
      }

      compute_id_coords(pid);

    }

    void create_by_k(int P, int pid) {
      int sq = static_cast<int>(sqrt(static_cast<double>(P)));
      for (int i=1; i<=sq; ++i) {
        int t = P/i;
        if (i*t == P) {
          C = i;
          R = t;
        }
      }

      S = 1;

      compute_id_coords(pid);
    }

    void create_by_i(int P, int pid) {
      int sq = static_cast<int>(sqrt(static_cast<double>(P)));
      for (int i=1; i<=sq; ++i) {
        int t = P/i;
        if (i*t == P) {
          C = i;
          S = t;
        }
      }

      R = 1;

      compute_id_coords(pid);
    }

    void create_by_j(int P, int pid) {
      int sq = static_cast<int>(sqrt(static_cast<double>(P)));
      for (int i=1; i<=sq; ++i) {
        int t = P/i;
        if (i*t == P) {
          S = i;
          R = t;
        }
      }

      C = 1;

      compute_id_coords(pid);
    }

  public:
    /** Constructor that takes the number of processes and the caller ID to produce the grid
        \param[in] c The object of the class used to specify periodicity in each dimension
        \param[in] P Number of processes that will make the grid
        \param[in] pid Number of processes that will make the grid
     */
    _3D_process_grid_t(period_type const &c, int P, int pid)
      : cyclic(c)
      , R(0)
      , C(0)
      , S(0)
      , r(0)
      , c(0)
      , s(0)
    {
      create(P,pid);
    }

    /** Function to create the grid. This can be called in case the
        grid is default constructed. Its direct use is discouraged.

        \param[in] P Number of processes that will make the grid
        \param[in] pid Number of processes that will make the grid
     */
    void create(int P, int pid) {
      char * pkind = getenv("GCL_3D_PROC_GRID");
      if (pkind == NULL) {
#ifndef NDEBUG
        std::cout << "Blocked grid of processes - env variable not set" << std::endl;
#endif
        create_blocked(P, pid);
        return;
      }
      std::string kind(pkind);
      boost::to_upper(kind);

      if ( kind == "BLOCKED" ) {
#ifndef NDEBUG
        std::cout << "Blocked grid of processes" << std::endl;
#endif
        create_blocked(P, pid);
      } else {
        if ( kind == "BY_I" ) {
#ifndef NDEBUG
          std::cout << "Grid of processes along directionn i" << std::endl;
#endif
          create_by_i(P, pid);
        } else {
          if ( kind == "BY_J" ) {
#ifndef NDEBUG
            std::cout << "Grid of processes along directionn j" << std::endl;
#endif
            create_by_i(P, pid);
          } else {
            if ( kind == "BY_K" ) {
#ifndef NDEBUG
              std::cout << "Grid of processes along directionn k" << std::endl;
#endif
              create_by_i(P, pid);
            } else {
              // default
#ifndef NDEBUG
              std::cout << "Blocked grid of processes (not recognized env. variable)" << std::endl;
#endif
              create_blocked(P, pid);
            }
          }
        }
      }
    }

    /** Returns in t_R and t_C the lenght of the dimensions of the process grid AS PRESCRIBED BY THE CONCEPT
        \param[out] t_R Number of elements in first dimension
        \param[out] t_C Number of elements in second dimension
        \param[out] t_S Number of elements in third dimension
    */
    void dims(int &t_R, int &t_C, int &t_S) const {
      t_R=R;
      t_C=C;
      t_S=S;
    }


    /** Returns the number of processors of the processor grid

        \return Number of processors
    */
    int size() const {
      return R*C*S;
    }


    /** Returns in t_R and t_C the coordinates ot the caller process in the grid AS PRESCRIBED BY THE CONCEPT
        \param[out] t_R Coordinate in first dimension
        \param[out] t_C Coordinate in second dimension
        \param[out] t_S Coordinate in third dimension
    */
    void coords(int &t_R, int &t_C, int &t_S) const {
      t_R = r;
      t_C = c;
      t_S = s;
    }

    /** Returns the process ID of the process with relative coordinates (I,J) with respect to the caller process AS PRESCRIBED BY THE CONCEPT
        \tparam I Relative coordinate in the first dimension
        \tparam J Relative coordinate in the seocnd dimension
        \tparam K Relative coordinate in the third dimension
        \return The process ID of the required process
    */
    template <int I, int J, int K>
    int proc() const {
      return proc(I,J,K);
    }

    /** Returns the process ID of the process with relative coordinates (I,J) with respect to the caller process AS PRESCRIBED BY THE CONCEPT
        \param[in] I Relative coordinate in the first dimension
        \param[in] J Relative coordinate in the seocnd dimension
        \param[in] K Relative coordinate in the third dimension
        \return The process ID of the required process
    */
    int proc(int I, int J, int K) const {
      int rr,cc,ss;
      if (cyclic.value0)
        rr = (R+r+I)%R;
      else
        rr = r+I;

      if (cyclic.value1)
        cc = (C+c+J)%C;
      else
        cc = c+J;

      if (cyclic.value2)
        ss = (S+s+K)%S;
      else
        ss = s+K;

      return proc_idx(rr, cc, ss);
    }

    /** Returns the process ID of the process with absolute coordinates specified by the input gridtools::array of coordinates
        \param[in] crds gridtools::aray of coordinates of the processor of which the ID is needed

        \return The process ID of the required process
    */
    int abs_proc(gridtools::array<int,ndims> const & crds) const {
      return proc(crds[0]-r, crds[1]-c, crds[2]-s);
    }

  };


#ifdef _GCL_MPI_
  /** \class MPI_3D_process_grid_t
   * Class that provides a representation of a 3D process grid given an MPI CART
   * It requires the MPI CART to be defined before the grid is created
   * \tparam CYCLIC is a template argument matching \ref boollist_concept to specify periodicities
   * \n
   * This is a process grid matching the \ref proc_grid_concept concept
   */
  template <typename CYCLIC>
  struct MPI_3D_process_grid_t {

    typedef boost::true_type has_communicator;
    typedef CYCLIC period_type;

    /** number of dimensions
     */
      static const int ndims = CYCLIC::size;

  private:
      MPI_Comm m_communicator; // Communicator that is associated with the MPI CART!
      period_type cyclic;
      int m_dimensions[ndims];
      int m_coordinates[ndims];
      int m_nprocs;
  public:
    /**
        Returns communicator
     */
      MPI_Comm communicator() const {
          return m_communicator;
      }

    /** Constructor that takes an MPI CART communicator, already configured, and use it to set up the process grid.
        \param c Object containing information about periodicities as defined in \ref boollist_concept
        \param comm MPI Communicator describing the MPI 3D computing grid
     */
      MPI_3D_process_grid_t(period_type const &c, MPI_Comm const& comm)
          :
          cyclic(c)
#if  !defined(__clang__) && defined(CXX11_ENABLED)
          ,m_dimensions{0},
          m_coordinates{0}
          {
#else
          {
              for (ushort_t i=0; i<ndims; ++i){
                  m_dimensions[i]=0;
                  m_coordinates[i]=0;
              }
#endif
          create(comm);}

    /** Function to create the grid. This can be called in case the
        grid is default constructed. Its direct use is discouraged

        \param comm MPI Communicator describing the MPI 3D computing grid
     */
    void create(MPI_Comm const& comm) {
        //int dims[ndims]={0,0,0}, periods[ndims]={true,true,true}, coords[ndims]={0,0,0};
        int period[ndims];
        period[0]=cyclic.value0;
        period[1]=cyclic.value1;
        period[2]=cyclic.value2;
        MPI_Comm_size(comm, &m_nprocs);
        MPI_Dims_create(m_nprocs, ndims, m_dimensions);
        MPI_Cart_create(comm, ndims, m_dimensions, period, false, &m_communicator);
        MPI_Cart_get(m_communicator, ndims, m_dimensions, period/*does not really care*/, m_coordinates);
      // for(ushort_t i=0; i<ndims; ++i)
      // {
      //     m_dimensions[i]=dims[i];
      //     m_coordinates[i] = coords[i];
      // }
    }

    /** Returns in t_R and t_C the lenght of the dimensions of the process grid AS PRESCRIBED BY THE CONCEPT
        \param[out] t_R Number of elements in first dimension
        \param[out] t_C Number of elements in second dimension
        \param[out] t_S Number of elements in third dimension
    */
    void dims(int &t_R, int &t_C, int &t_S) const {
        GRIDTOOLS_STATIC_ASSERT(ndims==3, "this interface supposes ndims=3")
        t_R=m_dimensions[0];
        t_C=m_dimensions[1];
        t_S=m_dimensions[2];
    }

      /** Returns the number of processors of the processor grid

        \return Number of processors
    */
    int size() const {
      return m_dimensions[0]*m_dimensions[1]*m_dimensions[2];
    }

    /** Returns in t_R and t_C the coordinates ot the caller process in the grid AS PRESCRIBED BY THE CONCEPT
        \param[out] t_R Coordinate in first dimension
        \param[out] t_C Coordinate in second dimension
        \param[out] t_S Coordinate in third dimension
    */
    void coords(int &t_R, int &t_C, int &t_S) const {
        GRIDTOOLS_STATIC_ASSERT(ndims==3, "this interface supposes ndims=3")
            t_R = m_coordinates[0];
        t_C = m_coordinates[1];
        t_S = m_coordinates[2];
    }

    /** Returns the process ID of the process with relative coordinates (I,J) with respect to the caller process AS PRESCRIBED BY THE CONCEPT
        \tparam I Relative coordinate in the first dimension
        \tparam J Relative coordinate in the second dimension
        \tparam K Relative coordinate in the third dimension
        \return The process ID of the required process
    */
      template <int I, int J, int K>
    int proc() const {
      return proc(I,J,K);
    }

      int pid() const {
          return m_coordinates[0]+m_coordinates[1]+m_coordinates[2];
      }

    /** Returns the process ID of the process with relative coordinates (I,J) with respect to the caller process AS PRESCRIBED BY THE CONCEPT
        \param[in] I Relative coordinate in the first dimension
        \param[in] J Relative coordinate in the second dimension
        \param[in] K Relative coordinate in the third dimension
        \return The process ID of the required process
    */
      int proc(int I, int J, int K) const {
      int _coords[3];

      if (cyclic.value0)
        _coords[0] = (m_coordinates[0]+I)%m_dimensions[0];
      else {
        _coords[0] = m_coordinates[0]+I;
        if (_coords[0]<0 || _coords[0]>=m_dimensions[0])
          return -1;
      }

      if (cyclic.value1)
        _coords[1] = (m_coordinates[1]+J)%m_dimensions[1];
      else {
        _coords[1] = m_coordinates[1]+J;
        if (_coords[1]<0 || _coords[1]>=m_dimensions[1])
          return -1;
      }

      if (cyclic.value2)
        _coords[2] = (m_coordinates[2]+K)%m_dimensions[2];
      else {
        _coords[2] = m_coordinates[2]+K;
        if (_coords[2]<0 || _coords[2]>=m_dimensions[2])
          return -1;
      }

      int res;
      MPI_Cart_rank(m_communicator, _coords, &res);
      return res;
    }

      int* coordinates(){return m_coordinates;}
      int* dimensions(){return m_dimensions;}
    /** Returns the process ID of the process with absolute coordinates specified by the input gridtools::array of coordinates
        \param[in] crds gridtools::aray of coordinates of the processor of which the ID is needed

        \return The process ID of the required process
    */
      int abs_proc(gridtools::array<int,ndims> const & crds) const {
          return proc(crds[0]-m_coordinates[0], crds[1]-m_coordinates[1], crds[2]-m_coordinates[2]);
    }

      int ntasks(){return m_nprocs;}

  };

#endif

} //namespace gridtools

#endif
