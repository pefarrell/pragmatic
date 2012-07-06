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

#include <iostream>
#include <vector>
#include <map>

#include <mpi.h>
#include "zoltan_colour.h"

using namespace std;

int main(int argc, char **argv){
  MPI::Init(argc,argv);

  /* 0: 3, 1
     1: 0, 2
     2: 1, 3
     3: 2, 0
  */
  zoltan_colour_graph_t graph;

  graph.rank = 0;

  /* Number of nodes in the graph assigned to the local process.
   */
  graph.npnodes = 4;
  
  /* Total number of nodes on local process.
   */
  graph.nnodes = 4;
  
  /* Array storing the number of edges connected to each node.
   */
  size_t nedges[] = {2, 2, 2, 2};
  graph.nedges = nedges;

  /* Array storing the edges in compressed row storage format.
   */
  size_t csr_edges[] = {3, 1, 0, 2, 1, 3, 2, 0};
  graph.csr_edges = csr_edges;
  
  /* Mapping from local node numbers to global node numbers.
   */
  int gid[] = {0, 1, 2, 3};
  graph.gid = gid;
  
  /* Process owner of each node.
   */
  size_t owner[] = {0, 0, 0, 0};
  graph.owner = owner;

  /* Graph colouring.
   */
  int colour[] = {0, 0, 0, 0};
  graph.colour = colour;

  zoltan_colour(&graph, 1, MPI_COMM_WORLD);
  
  if((graph.colour[0]==1)&&(graph.colour[1]==2)&&
     (graph.colour[2]==1)&&(graph.colour[3]==2))
    cout<<"pass\n";
  else{
    cout<<"Colouring = ";
    for(int i=0;i<4;i++)
      cout<<graph.colour[i]<<" ";
    cout<<endl;
    cout<<"fail\n";
  }

  MPI::Finalize();

  return 0;
}
