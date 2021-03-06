/*  Copyright (C) 2010 Imperial College London and others.
 *
 *  Please see the AUTHORS file in the main source directory for a
 *  full list of copyright holders.
 *
 *  Gerard Gorman
 *  Applied Modelling and Computation Group
 *  Department of Earth Science and Engineering
 *  Imperial College London
 *
 *  g.gorman@imperial.ac.uk
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following
 *  disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 *  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */

#include <cmath>
#include <iostream>
#include <vector>

#include <omp.h>

#include "Mesh.h"
#include "VTKTools.h"
#include "MetricField.h"

#include "Coarsen.h"
#include "Refine.h"
#include "Smooth.h"
#include "Swapping.h"
#include "ticker.h"

#include <mpi.h>

int main(int argc, char **argv){
  int required_thread_support=MPI_THREAD_SINGLE;
  int provided_thread_support;
  MPI_Init_thread(&argc, &argv, required_thread_support, &provided_thread_support);
  assert(required_thread_support==provided_thread_support);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  bool verbose = false;
  if(argc>1){
    verbose = std::string(argv[1])=="-v";
  }

  const double pi = 3.141592653589793;
  const double period = 100.0;

  // Benchmark times.
  double time_coarsen=0, time_refine=0, time_swap=0, time_smooth=0, time_adapt=0;

  Mesh<double> *mesh=VTKTools<double>::import_vtu("../data/box50x50x50.vtu");
  mesh->create_boundary();

  double eta=0.05;
  char filename[4096];

  if(rank==0)
    std::cout<<"BENCHMARK: time_coarsen time_refine time_swap time_smooth time_adapt\n";
  for(int t=0;t<51;t++){
    size_t NNodes = mesh->get_number_nodes();

    MetricField<double,3> metric_field(*mesh);

    for(size_t i=0;i<NNodes;i++){
      double x = 2*mesh->get_coords(i)[0]-1;
      double y = 2*mesh->get_coords(i)[1]-1;
      double z = 2*mesh->get_coords(i)[2]-1;

      double m[] = {0.2*(-8*x + 4*sin(5*y+2*pi*t/period))/pow(pow(2*x - sin(5*y+2*pi*t/period), 2) + 0.01, 2) - 250.0*sin(50*x+2*pi*t/period),
          2.0*(2*x - sin(5*y+2*pi*t/period))*cos(5*y+2*pi*t/period)/pow(pow(2*x - sin(5*y+2*pi*t/period), 2) + 0.01, 2),
          0,
          -5.0*(2*x - sin(5*y+2*pi*t/period))*pow(cos(5*y+2*pi*t/period), 2)/pow(pow(2*x - sin(5*y+2*pi*t/period), 2) + 0.01, 2) + 2.5*sin(5*y+2*pi*t/period)/(pow(2*x - sin(5*y+2*pi*t/period), 2) + 0.01),
          0,
          0};

      for(int j=0;j<5;j++)
        m[j]/=eta;
      m[5] = 1.0;

      metric_field.set_metric(m, i);
    }

    metric_field.apply_max_aspect_ratio(10);
    metric_field.update_mesh();

    if(verbose){
      sprintf(filename, "../data/benchmark_adapt_3d-init-%d", t);
      VTKTools<double>::export_vtu(&(filename[0]), mesh);
    }
    double T1 = get_wtime();

    // See Eqn 7; X Li et al, Comp Methods Appl Mech Engrg 194 (2005) 4915-4950
    double L_up = sqrt(2.0);
    double L_low = L_up/2;

    Coarsen<double,3> coarsen(*mesh);
    Smooth<double,3> smooth(*mesh);
    Refine<double,3> refine(*mesh);
    Swapping<double,3> swapping(*mesh);

    double tic, toc;

    double L_max = mesh->maximal_edge_length();
    double alpha = sqrt(2.0)/2;

    for(size_t I=0;I<5;I++){
      for(size_t i=0;i<10;i++){
        double L_ref = std::max(alpha*L_max, L_up);

        tic = get_wtime();
        coarsen.coarsen(L_low, L_ref);
        toc = get_wtime();
        if(t>0) time_coarsen += (toc-tic);
        if(verbose){
          std::cout<<"INFO: Verify quality after coarsen.\n";
          mesh->verify();
          long double area = mesh->calculate_area();
          long double volume = mesh->calculate_volume();
          std::cout<<"Expecting area == 6: ";
          if(fabs(area-6)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (area="<<area<<")"<<std::endl;
          std::cout<<"Expecting volume == 1: ";
          if(fabs(volume-1)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (volume="<<volume<<")"<<std::endl;
        }

        tic = get_wtime();
        swapping.swap(0.7);
        toc = get_wtime();
        if(t>0) time_swap += (toc-tic);
        if(verbose){
          std::cout<<"INFO: Verify quality after swapping.\n";
          mesh->verify();
          long double area = mesh->calculate_area();
          long double volume = mesh->calculate_volume();
          std::cout<<"Expecting area == 6: ";
          if(fabs(area-6)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (area="<<area<<")"<<std::endl;
          std::cout<<"Expecting volume == 1: ";
          if(fabs(volume-1)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (volume="<<volume<<")"<<std::endl;
        }

        tic = get_wtime();
        refine.refine(L_ref);
        toc = get_wtime();
        if(t>0) time_refine += (toc-tic);
        if(verbose){
          std::cout<<"INFO: Verify quality after refinement.\n";
          mesh->verify();
          long double area = mesh->calculate_area();
          long double volume = mesh->calculate_volume();
          std::cout<<"Expecting area == 6: ";
          if(fabs(area-6)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (area="<<area<<")"<<std::endl;
          std::cout<<"Expecting volume == 1: ";
          if(fabs(volume-1)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (volume="<<volume<<")"<<std::endl;
        }

        L_max = mesh->maximal_edge_length();

        if((L_max-L_up)<0.01)
          break;
      }

      mesh->defragment();

      tic = get_wtime();
      if(I>0){
        smooth.smart_laplacian(I*10, 1.0);
        if(verbose){
          std::cout<<"After smart Laplacian smoothing:\n";
          mesh->verify();
          long double area = mesh->calculate_area();
          long double volume = mesh->calculate_volume();
          std::cout<<"Expecting area == 6: ";
          if(fabs(area-6)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (area="<<area<<")"<<std::endl;
          std::cout<<"Expecting volume == 1: ";
          if(fabs(volume-1)<DBL_EPSILON)
            std::cout<<"pass"<<std::endl;
          else
            std::cout<<"fail (volume="<<volume<<")"<<std::endl;
        }
      }
      smooth.optimisation_linf(10);
      toc = get_wtime();
      if(t>0) time_smooth += (toc-tic);
      if(verbose){
        std::cout<<"After optimisation based smoothing:\n";
        mesh->verify();
        long double area = mesh->calculate_area();
        long double volume = mesh->calculate_volume();
        std::cout<<"Expecting area == 6: ";
        if(fabs(area-6)<DBL_EPSILON)
          std::cout<<"pass"<<std::endl;
        else
          std::cout<<"fail (area="<<area<<")"<<std::endl;
        std::cout<<"Expecting volume == 1: ";
        if(fabs(volume-1)<DBL_EPSILON)
          std::cout<<"pass"<<std::endl;
        else
          std::cout<<"fail (volume="<<volume<<")"<<std::endl;
      }

      if(mesh->get_qmin()>0.4)
        break;
    }

    double T2 = get_wtime();
    if(t>0) time_adapt += (T2-T1);

    if((t>0)&&(rank==0))
      std::cout<<"BENCHMARK: "
               <<std::setw(12)<<time_coarsen/t<<" "
               <<std::setw(11)<<time_refine/t<<" "
               <<std::setw(9)<<time_swap/t<<" "
               <<std::setw(11)<<time_smooth/t<<" "
               <<std::setw(10)<<time_adapt/t<<std::endl
               <<"NNodes, NElements, t = "<<mesh->get_number_nodes()<<", "<<mesh->get_number_elements()<< ", " << t <<std::endl;

    if(verbose){
      mesh->print_quality();

      std::cerr<<t<<" :: meatgrinder "<<mesh->get_qmin()<<std::endl;

      sprintf(filename, "../data/benchmark_adapt_3d-%d", t);
      VTKTools<double>::export_vtu(&(filename[0]), mesh);
    }
  }

  delete mesh;

  MPI_Finalize();

  return 0;
}
