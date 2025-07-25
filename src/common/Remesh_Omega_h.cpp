#include "ReMesher.h"

#include <Omega_h_adapt.hpp>
#include <Omega_h_amr.hpp>
#include <Omega_h_array_ops.hpp>
#include <Omega_h_build.hpp>
#include <Omega_h_for.hpp>
#include <Omega_h_mesh.hpp>
#include <Omega_h_file.hpp>
#include <Omega_h_quality.hpp>
#include <Omega_h_map.hpp> //colloect matrked
#include <Omega_h_metric.hpp>
#include <Omega_h_timer.hpp>
#include <Omega_h_timer.hpp>
#include <Omega_h_coarsen.hpp>
#include "Omega_h_refine_qualities.hpp"
#include "Omega_h_array_ops.hpp"      /// ARE CLOSE
#include "Omega_h_class.hpp"
#include "Omega_h_quality.hpp"
#include "Omega_h_vector.hpp"
#include <Omega_h_transfer.hpp>

#ifdef CUDA_BUILD
#include <cuda_runtime.h>
#endif

namespace MetFEM{
  
void classify_vertices(Omega_h::Mesh* mesh) {
    // Assume all vertices are in the interior (volume = 3)
    Omega_h::Write<Omega_h::I8> class_dim(mesh->nverts(), 3);

    // If you have known boundary conditions, modify class_dim accordingly
    // Example: If you have a function that detects surface nodes, set them to 2

    // Attach classification data to mesh
    mesh->add_tag(Omega_h::VERT, "class_dim", 1, Omega_h::Read<Omega_h::I8>(class_dim));
}


using namespace Omega_h;
template <Int dim>
void create_class_dim(Mesh* mesh) {
    // Number of vertices in the mesh
    auto nverts = mesh->nverts();
    
    // Retrieve the coordinates of the vertices
    auto coords = mesh->coords();
    
    // Create a Write<Byte> container for storing the classification results
    auto class_dims = Write<Byte>(nverts);

    // Define some boundary criteria (for example, 0.5 could represent a boundary threshold)
    Real boundary_threshold = 0.5;

    // Loop over all the vertices to classify them
    auto f = OMEGA_H_LAMBDA(LO v) {
        // Get the coordinates of the current vertex (assuming 3D mesh here)
        auto x = get_vector<dim>(coords, v);
        
        // Simple classification logic based on some threshold (e.g., x[0] > boundary_threshold)
        Byte class_dim = 0; // Initially set to "interior"
        
        // For example, if the vertex is close to the boundary, we classify it as boundary
        if (x[0] > boundary_threshold) {
            class_dim = 1;  // "boundary"
        }
        
        // Store the classification result in the Write<Byte> container
        class_dims[v] = class_dim;
    };

    // Run the classification function in parallel
    parallel_for(nverts, f, "classify_vertices");

    // Add the class_dim tag to the mesh
    mesh->add_tag<Byte>(VERT, "class_dim", 1, class_dims);
}


template <Int dim>
void classify_faces(Mesh* mesh) {
    // Number of faces in the mesh
    auto nfaces = mesh->nfaces();

    // Retrieve the coordinates of the faces or the centroids of faces
    //auto centroids = mesh->centroids();

    // Create a Write<Byte> container to store the classification results
    auto class_dims = Write<Byte>(nfaces,0);

    // Define some classification logic, for example, boundary faces are classified as 1
    Real boundary_threshold = 0.5;
    /*
    // Loop over all the faces (triangles, quadrilaterals, etc.)
    auto f = OMEGA_H_LAMBDA(LO f) {
        // Get the centroid of the current face (assuming 3D mesh here)
        auto x = get_vector<dim>(centroids, f);
        
        // Initialize classification to interior (0)
        Byte class_dim = 0;

        // Example condition: If the centroid is close to the boundary, classify it as boundary (1)
        if (x[0] > boundary_threshold) {
            class_dim = 1; // Boundary
        }

        // Store the classification result in the Write<Byte> container
        class_dims[f] = class_dim;
    };
    */

    // Run the classification function in parallel
    //parallel_for(nfaces, f, "classify_faces");

    // Add the class_dim tag to the mesh for faces
    mesh->add_tag<Byte>(FACE, "class_dim", 1, class_dims);
}

template <Int dim>
void classify_edges(Mesh* mesh) {
    // Number of edges in the mesh
    auto nedges = mesh->nedges();

    // Create a Write<Byte> container to store the classification results
    auto class_dims = Write<Byte>(nedges, 0);

    // Define some classification logic, for example, boundary edges are classified as 1
    Real boundary_threshold = 0.5;

    /*
    // Loop over all the edges
    auto e = OMEGA_H_LAMBDA(LO e) {
        // Get the midpoint of the current edge (assuming 3D mesh here)
        auto x = get_vector<dim>(edge_midpoints, e);
        
        // Initialize classification to interior (0)
        Byte class_dim = 0;

        // Example condition: If the midpoint is close to the boundary, classify it as boundary (1)
        if (x[0] > boundary_threshold) {
            class_dim = 1; // Boundary
        }

        // Store the classification result in the Write<Byte> container
        class_dims[e] = class_dim;
    };
    */

    // Run the classification function in parallel
    // parallel_for(nedges, e, "classify_edges");

    // Add the class_dim tag to the mesh for edges
    mesh->add_tag<Byte>(EDGE, "class_dim", 1, class_dims);
}

void create_mesh(Omega_h::Mesh& mesh, 
#ifdef CUDA_BUILD
                 double* d_node_coords, int num_nodes, 
                 int* d_element_conn, int num_elements
#else
                 double* h_node_coords, int num_nodes, 
                 int* h_element_conn, int num_elements
#endif
                 ) 
{
#ifdef CUDA_BUILD
    // GPU Case: Use Omega_h::Write<> with device pointers
    Omega_h::Write<Omega_h::Real> device_coords(d_node_coords, num_nodes * 3);
    Omega_h::Write<Omega_h::LO> device_tets(d_element_conn, num_elements * 4);
#else
    #ifdef DEBUG_MODE
    std::cout << "Creating "<< num_nodes<< " nodes "<<std::endl;
    #endif
    // CPU Case: Copy raw pointer data to Omega_h::HostWrite<>
    Omega_h::HostWrite<Omega_h::Real> coords(num_nodes * 3);
    Omega_h::HostWrite<Omega_h::LO> tets(num_elements * 4);

    //Omega_h::Write<Omega_h::Real> coords(num_nodes * 3);
    //Omega_h::Write<Omega_h::LO> tets(num_elements * 4);

    //std::cout << "Done "<<std::endl;    
    for (int i = 0; i < num_nodes * 3; ++i) coords[i] = h_node_coords[i];
    for (int i = 0; i < num_elements * 4; ++i) tets[i] = h_element_conn[i];
    
    //cout << "ELEM CONN "<<h_element_conn[0]<<endl;

    //std::cout << "Convert to write "<<std::endl;    
    // Convert HostWrite to Write<> for Omega_h
    Omega_h::Write<Omega_h::Real> device_coords(coords);
    Omega_h::Write<Omega_h::LO> device_tets(tets);
#endif
    #ifdef DEBUG_MODE
    std::cout << "Building from elements "<<std::endl;
    #endif
    // Build mesh (works on both CPU and GPU)
    build_from_elems_and_coords(&mesh,OMEGA_H_SIMPLEX, 3, device_tets, device_coords); // Correct method

  if (!mesh.has_tag(Omega_h::VERT, "coordinates")) {
      std::cerr << "Error: Mesh does not have 'coordinates' tag!" << std::endl;
  }
  classify_elements(&mesh);
  classify_faces<3>(&mesh);
  classify_edges<3>(&mesh);
  classify_vertices(&mesh);
  create_class_dim<3>(&mesh);
  
if (!mesh.has_tag(Omega_h::VERT, "class_dim")) {
    std::cerr << "Error: Mesh does not have 'class_dim' tag!" << std::endl;
}
    // Step 2: Add the node coordinates as a tag (e.g., "coords")
    //mesh.set_tag(Omega_h::VERT, "metric", Omega_h::Reals(device_coords));

    // Step 3: Add element connectivity as a tag (e.g., "conn")
    //mesh.set_tag(Omega_h::CELL, "conn", Omega_h::Reals(device_tets));

    // Optionally, you can also print out to verify the number of nodes and elements
    //std::cout << "Mesh created with " << mesh.nverts() << " vertices and "
    //          << mesh.nelems() << " elements.\n";
}


// Function to evaluate if refinement is necessary based on the scalar field (e.g., plastic strain)
bool needs_refinement(double scalar_value, double threshold) {
    return scalar_value > threshold;
}

using namespace Omega_h;

/////// FROM UNIT_MESH (several tests)
void refine_mesh_quality(Omega_h::Mesh &mesh){
  //build_box_internal(&mesh, OMEGA_H_SIMPLEX, 1., 1., 0., 1, 1, 0);
  LOs candidates(mesh.nedges(), 0, 1);
  mesh.add_tag(VERT, "metric", symm_ncomps(2),
      repeat_symm(mesh.nverts(), identity_matrix<2, 2>()));
  auto quals = refine_qualities(&mesh, candidates);
  OMEGA_H_CHECK(are_close(
      quals, Reals({0.494872, 0.494872, 0.866025, 0.494872, 0.494872}), 1e-4));
}
//////////////////////////// FROM UGAWG LINEAR


template <int dim>
static void set_target_metric(Mesh* mesh) {
  auto coords = mesh->coords();
  auto target_metrics_w = Write<Real>(mesh->nverts() * symm_ncomps(dim));
  auto f = OMEGA_H_LAMBDA(LO v) {
    auto z = coords[v * dim + (dim - 1)];
    auto h = Vector<dim>();
    for (Int i = 0; i < dim - 1; ++i) h[i] = 0.1;
    h[dim - 1] = 0.001 + 0.198 * std::abs(z - 0.5);
    auto m = diagonal(metric_eigenvalues_from_lengths(h));
    set_symm(target_metrics_w, v, m);
  };
  parallel_for(mesh->nverts(), f);
  mesh->set_tag(VERT, "target_metric", Reals(target_metrics_w));
}

template <Int dim>
void run_case(Mesh* mesh, char const* vtk_path) {
  auto world = mesh->comm();
  mesh->set_parting(OMEGA_H_GHOSTED);
  auto implied_metrics = get_implied_metrics(mesh);
  mesh->add_tag(VERT, "metric", symm_ncomps(dim), implied_metrics);
  std::cout << "symm_ncomps(" << dim << ") = " << symm_ncomps(dim) << std::endl;
  mesh->add_tag<Real>(VERT, "target_metric", symm_ncomps(dim));
  set_target_metric<dim>(mesh);
  mesh->set_parting(OMEGA_H_ELEM_BASED);
  mesh->ask_lengths();
  mesh->ask_qualities();
  vtk::FullWriter writer;
  if (vtk_path) {
    writer = vtk::FullWriter(vtk_path, mesh);
    writer.write();
  }
  auto opts = AdaptOpts(mesh);
  opts.verbosity = EXTRA_STATS;
  opts.length_histogram_max = 2.0;
  opts.max_length_allowed = opts.max_length_desired * 2.0;
  Now t0 = now();
  std::cout << "Adapting "<<std::endl; 
  while (approach_metric(mesh, opts)) {
    std::cout << "Step "<<std::endl;
    adapt(mesh, opts);
    std::cout << "DONE"<<std::endl;
    //if (mesh->has_tag(VERT, "target_metric")) set_target_metric<dim>(mesh);
    //else      std::cerr << "Error: target_metric tag was not properly set!" << std::endl;
    //if (vtk_path) writer.write();
  }
  Now t1 = now();
  std::cout << "total time: " << (t1 - t0) << " seconds\n";
}

////////////////////////////////////////////////////////////////////////////////////////////
/// FROM AMR_TEST2

template <int dim>
OMEGA_H_INLINE double eval_rc(Omega_h::Vector<dim> c);

template <>
OMEGA_H_INLINE double eval_rc<2>(Omega_h::Vector<2> c) {
  auto rc2 = (c[0] - 0.5) * (c[0] - 0.5) + (c[1] - 0.5) * (c[1] - 0.5);
  return std::sqrt(rc2);
}

template <>
OMEGA_H_INLINE double eval_rc<3>(Omega_h::Vector<3> c) {
  auto rc2 = (c[0] - 0.5) * (c[0] - 0.5) + (c[1] - 0.5) * (c[1] - 0.5) +
             (c[2] - 0.5) * (c[2] - 0.5);
  return std::sqrt(rc2);
}


template <int dim>
Omega_h::Bytes mark(Omega_h::Mesh* m, int level) {
  auto coords = m->coords();
  auto mids = Omega_h::average_field(m, dim, dim, coords);
  auto is_leaf = m->ask_leaves(dim);
  auto leaf_elems = Omega_h::collect_marked(is_leaf);
  Omega_h::Write<Omega_h::Byte> marks(m->nelems(), 0);
  auto f = OMEGA_H_LAMBDA(Omega_h::LO e) {
    auto elem = leaf_elems[e];
    auto c = Omega_h::get_vector<dim, Omega_h::Reals>(mids, elem);
    auto rc = eval_rc<dim>(c);
    auto r = 0.25;
    auto tol = 0.314 / static_cast<double>(level);
    if (std::abs(rc - r) < tol) marks[elem] = 1;
  };
  Omega_h::parallel_for(leaf_elems.size(), f);
  return Omega_h::amr::enforce_2to1_refine(m, dim - 1, marks);
}


void refine(Mesh* mesh){
  Omega_h::vtk::Writer writer("out_amr_3D", mesh);
  writer.write();
  for (int i = 1; i < 5; ++i) {
    auto xfer_opts = Omega_h::TransferOpts();
    auto marks = mark<3>(mesh, i);
    Omega_h::amr::refine(mesh, marks, xfer_opts);
    writer.write();
  }
}


//////////////////////////////////////////////////////////////////////////////////////
// FOM WARP

using namespace Omega_h;

void add_dye(Mesh* mesh) {
  auto dye_w = Write<Real>(mesh->nverts());
  auto coords = mesh->coords();
  auto dye_fun = OMEGA_H_LAMBDA(LO vert) {
    auto x = get_vector<3>(coords, vert);
    auto left_cen = vector_3(.25, .5, .5);
    auto right_cen = vector_3(.75, .5, .5);
    auto left_dist = norm(x - left_cen);
    auto right_dist = norm(x - right_cen);
    auto dist = min2(left_dist, right_dist);
    if (dist < .25) {
      auto dir = sign(left_dist - right_dist);
      dye_w[vert] = 4.0 * dir * (.25 - dist);
    } else {
      dye_w[vert] = 0;
    }
  };
  parallel_for(mesh->nverts(), dye_fun);
  mesh->add_tag(VERT, "dye", 1, Reals(dye_w));
}

Reals form_pointwise(Mesh* mesh) {
  auto dim = mesh->dim();
  auto ecoords =
      average_field(mesh, dim, LOs(mesh->nelems(), 0, 1), dim, mesh->coords());
  auto pw_w = Write<Real>(mesh->nelems());
  auto pw_fun = OMEGA_H_LAMBDA(LO elem) { pw_w[elem] = ecoords[elem * dim]; };
  parallel_for(mesh->nelems(), pw_fun);
  return pw_w;
}

static void add_pointwise(Mesh* mesh) {
  auto data = form_pointwise(mesh);
  mesh->add_tag(mesh->dim(), "pointwise", 1, data);
}

static void check_total_mass(Mesh* mesh) {
  auto densities = mesh->get_array<Real>(mesh->dim(), "density");
  auto sizes = mesh->ask_sizes();
  Reals masses = multiply_each(densities, sizes);
  auto owned_masses = mesh->owned_array(mesh->dim(), masses, 1);
  OMEGA_H_CHECK(are_close(1.0, get_sum(mesh->comm(), owned_masses)));
}

static void postprocess_pointwise(Mesh* mesh) {
  auto data = mesh->get_array<Real>(mesh->dim(), "pointwise");
  auto expected = form_pointwise(mesh);
  auto diff = subtract_each(data, expected);
  mesh->add_tag(mesh->dim(), "pointwise_err", 1, diff);
}

template <int dim>
void adapt_warp(Mesh &mesh){

 mesh.set_parting(OMEGA_H_GHOSTED);
  auto metrics = get_implied_isos(&mesh);
  mesh.add_tag(VERT, "metric", 1, metrics);
  add_dye(&mesh);
  mesh.add_tag(mesh.dim(), "density", 1, Reals(mesh.nelems(), 1.0));
  add_pointwise(&mesh);
  auto opts = AdaptOpts(&mesh);
  opts.xfer_opts.type_map["density"] = OMEGA_H_CONSERVE;
  opts.xfer_opts.integral_map["density"] = "mass";
  opts.xfer_opts.type_map["pointwise"] = OMEGA_H_POINTWISE;
  opts.xfer_opts.type_map["dye"] = OMEGA_H_LINEAR_INTERP;
  opts.xfer_opts.integral_diffuse_map["mass"] = VarCompareOpts::none();
  opts.verbosity = EXTRA_STATS;
  auto mid = zero_vector<dim>();
  mid[0] = mid[1] = .5;
  Now t0 = now();
  for (Int i = 0; i < 8; ++i) {
    std::cout << "-------STEP "<<i<<std::endl;
    auto coords = mesh.coords();
    Write<Real> warp_w(mesh.nverts() * dim);
    auto warp_fun = OMEGA_H_LAMBDA(LO vert) {
      auto x0 = get_vector<3>(coords, vert);
      auto x1 = zero_vector<3>();
      x1[0] = x0[0];
      x1[1] = x0[1];
      auto x2 = x1 - mid;
      auto polar_a = std::atan2(x2[1], x2[0]);
      auto polar_r = norm(x2);
      Real rot_a = 0;
      if (polar_r < 0.5) {
        rot_a = (PI / 8) * (2.0 * (0.5 - polar_r));
        if (i >= 4) rot_a = -rot_a;
      }
      auto dest_a = polar_a + rot_a;
      auto dst = x0;
      dst[0] = std::cos(dest_a) * polar_r;
      dst[1] = std::sin(dest_a) * polar_r;
      dst = dst + mid;
      auto w = dst - x0;
      set_vector<3>(warp_w, vert, w);
    };
    parallel_for(mesh.nverts(), warp_fun);
    mesh.add_tag(VERT, "warp", dim, Reals(warp_w));
    while (warp_to_limit(&mesh, opts)) {
      adapt(&mesh, opts);
    }
  }
  Now t1 = now();
  mesh.set_parting(OMEGA_H_ELEM_BASED);
  if (mesh.comm()->rank() == 0) {
    std::cout << "test took " << (t1 - t0) << " seconds\n";
  }
  //check_total_mass(&mesh); //////CRASH
  postprocess_pointwise(&mesh);
  bool ok = check_regression("gold_warp", &mesh);  
}

template <int dim>
void adapt_warp_with_threshold(Mesh &mesh, Real length_threshold, Real angle_threshold) {
    mesh.set_parting(OMEGA_H_GHOSTED);
    auto metrics = get_implied_isos(&mesh);
    mesh.add_tag(VERT, "metric", 1, metrics);
    add_dye(&mesh);
    mesh.add_tag(mesh.dim(), "density", 1, Reals(mesh.nelems(), 1.0));
    add_pointwise(&mesh);
    auto opts = AdaptOpts(&mesh);
    opts.xfer_opts.type_map["density"] = OMEGA_H_CONSERVE;
    opts.xfer_opts.integral_map["density"] = "mass";
    opts.xfer_opts.type_map["pointwise"] = OMEGA_H_POINTWISE;
    opts.xfer_opts.type_map["dye"] = OMEGA_H_LINEAR_INTERP;
    opts.xfer_opts.integral_diffuse_map["mass"] = VarCompareOpts::none();
    //opts.xfer_opts.max_edge_length = length_threshold;
    opts.verbosity = EXTRA_STATS;

    auto mid = zero_vector<dim>();
    mid[0] = mid[1] = .5;
    Now t0 = now();


    opts.min_quality_allowed = 1.0e-3;  // Prevent bad-quality elements
    opts.should_refine = true;  // Allow refinement
    opts.should_coarsen = false;  // Allow coarsening
    
    for (Int i = 0; i < 8; ++i) {
        auto coords = mesh.coords();
        Write<Real> warp_w(mesh.nverts() * dim);
        auto warp_fun = OMEGA_H_LAMBDA(LO vert) {
            auto x0 = get_vector<3>(coords, vert);
            auto x1 = zero_vector<3>();
            x1[0] = x0[0];
            x1[1] = x0[1];
            auto x2 = x1 - mid;
            auto polar_a = std::atan2(x2[1], x2[0]);
            auto polar_r = norm(x2);
            Real rot_a = 0;
            if (polar_r < 0.5) {
                rot_a = (PI / 8) * (2.0 * (0.5 - polar_r));
                if (i >= 4) rot_a = -rot_a;
            }
            auto dest_a = polar_a + rot_a;
            auto dst = x0;
            dst[0] = std::cos(dest_a) * polar_r;
            dst[1] = std::sin(dest_a) * polar_r;
            dst = dst + mid;
            auto w = dst - x0;
            set_vector<3>(warp_w, vert, w);
        };
        parallel_for(mesh.nverts(), warp_fun);
        mesh.add_tag(VERT, "warp", dim, Reals(warp_w));

        // Compute element edge lengths
        auto elems2verts = mesh.ask_down(dim, VERT);
        auto vert_coords = mesh.coords();
        bool refine_needed = false;
    
    std::string name = "out_amr_length_3D_STEP_"+std::to_string(i);
    Omega_h::vtk::Writer writer2(name.c_str(), &mesh);
    writer2.write();
    
    //std::cout << "New mesh verts : "<<mesh.nverts()<<std::endl;
    //std::cout << "New mesh elems ; "<<mesh.nelems()<<std::endl;
    //opts.max_length_desired = 0.8;  // If an element is too big, split it
                    
        for (LO elem = 0; elem < mesh.nelems(); ++elem) {
            std::cout << "ELEMENT "<<elem<<std::endl; 
            for (int j = 0; j < dim + 1; ++j) {
                for (int k = j + 1; k < dim + 1; ++k) {
                    auto vj = elems2verts.ab2b[elem * (dim + 1) + j];
                    auto vk = elems2verts.ab2b[elem * (dim + 1) + k];
                    auto pj = get_vector<dim>(vert_coords, vj);
                    auto pk = get_vector<dim>(vert_coords, vk);
                    auto edge_length = norm(pk - pj);
                    std::cout << "Edge Length"<<edge_length<<std::endl;
                    if (edge_length > length_threshold) {
                        refine_needed = true;
                        break;
                    }
                }
                if (refine_needed) break;
            }
            if (refine_needed) break;
            if (refine_needed) std::cout << "REFINE "<<std::endl;
        }//Elem
        
        if (refine_needed) {
          std::cout << "Adapting: adapt(&mesh, opts);"<<std::endl;
          adapt(&mesh, opts);
          /*
            while (warp_to_limit(&mesh, opts)) {
                adapt(&mesh, opts);
            }
        */
        }
    }
    Now t1 = now();
    mesh.set_parting(OMEGA_H_ELEM_BASED);
    if (mesh.comm()->rank() == 0) {
        std::cout << "test took " << (t1 - t0) << " seconds\n";
    }
    //check_total_mass(&mesh);
    postprocess_pointwise(&mesh);
    bool ok = check_regression("gold_warp", &mesh);
    std::cout << "New mesh verts : "<<mesh.nverts()<<std::endl;
    std::cout << "New mesh elems ; "<<mesh.nelems()<<std::endl;
    std::cout << "Mesh adaptation complete based on temperature gradient and mesh density." << std::endl;
}


/*
void refine_mesh_quality(Omega_h::Mesh* mesh, double quality_threshold) {
  int dim = 3;
  auto world = mesh->comm();
  mesh->set_parting(OMEGA_H_GHOSTED);
  auto implied_metrics = get_implied_metrics(mesh);
  mesh->add_tag(VERT, "metric", symm_ncomps(dim), implied_metrics);
  mesh->add_tag<Real>(VERT, "target_metric", symm_ncomps(dim));
  set_target_metric<dim>(mesh);
  mesh->set_parting(OMEGA_H_ELEM_BASED);
  mesh->ask_lengths();
  mesh->ask_qualities();
  vtk::FullWriter writer;
  if (vtk_path) {
    writer = vtk::FullWriter(vtk_path, mesh);
    writer.write();
  }
  auto opts = AdaptOpts(mesh);
  opts.verbosity = EXTRA_STATS;
  opts.length_histogram_max = 2.0;
  opts.max_length_allowed = opts.max_length_desired * 2.0;
  Now t0 = now();
  while (approach_metric(mesh, opts)) {
    adapt(mesh, opts);
    if (mesh->has_tag(VERT, "target_metric")) set_target_metric<dim>(mesh);
    if (vtk_path) writer.write();
  }
  Now t1 = now();
  std::cout << "total time: " << (t1 - t0) << " seconds\n";
}
*/

template <int dim>
Real dot(const Omega_h::Vector<dim>& a, const Omega_h::Vector<dim>& b) {

    Real result = 0.0;
    for (int i = 0; i < dim; ++i) {
        result += a[i] * b[i];
    }
    return result;
}


#include <algorithm>  // For std::clamp
template <typename T>
T clamp(const T& value, const T& low, const T& high) {
    return (value < low) ? low : (value > high) ? high : value;
}
using namespace std;

template <int dim>
void adapt_with_thresholds(Mesh &mesh, Real length_threshold, Real angle_threshold) {
    cout <<"Initializig "<<endl;
        auto elems2verts = mesh.ask_down(dim, VERT);
        auto vert_coords = mesh.coords();

        cout << "Element conn size "<< elems2verts.ab2b.size()<<endl;
    //CHECK FOR DEGENERATE ANGLES
    /*
    for (LO elem = 0; elem < mesh.nelems(); ++elem) {
        //cout << "Element "<<elem<<endl;

        auto v1 = elems2verts.ab2b[elem * (dim + 1) ];
        auto v2 = elems2verts.ab2b[elem * (dim + 1) + 1];
        //cout << "v1 "<<v1<<endl;

        auto p1 = get_vector<dim>(vert_coords, v1);
        auto p2 = get_vector<dim>(vert_coords, v2);
        cout << "p1 "<<p1[0]<<endl;            
        //auto v1 = elems2verts.ab2b[elem * (dim + 1) + j];
        //auto v2 = elems2verts.ab2b[elem * (dim + 1) + k];
        
        auto edge_length = norm(p2 - p1);
        //cout << "Edge length "<<edge_length<<endl;
        if (edge_length <= 0.0) {
            std::cerr << "Degenerate element found: element " << elem << std::endl;
        }
    }   
*/    

    
    mesh.set_parting(OMEGA_H_GHOSTED);
    auto metrics = get_implied_isos(&mesh);
    mesh.add_tag(VERT, "metric", 1, metrics);
    //add_dye(&mesh);
    
   
    mesh.add_tag(mesh.dim(), "density", 1, Reals(mesh.nelems(), 1.0));
    add_pointwise(&mesh);
    
    auto opts = AdaptOpts(&mesh);
    opts.xfer_opts.type_map["density"] = OMEGA_H_CONSERVE;
    opts.xfer_opts.integral_map["density"] = "mass";
    opts.xfer_opts.type_map["pointwise"] = OMEGA_H_POINTWISE;
    //opts.xfer_opts.type_map["dye"] = OMEGA_H_LINEAR_INTERP;
    opts.xfer_opts.integral_diffuse_map["mass"] = VarCompareOpts::none();
    opts.verbosity = EXTRA_STATS;

    opts.min_quality_allowed = 1.0e-3;  // Prevent bad-quality elements
    opts.should_refine = true;  // Allow refinement
    opts.should_coarsen = false;  // Allow coarsening

    opts.max_length_desired = 1.2 * length_threshold;  // If an element is too big, split it
    opts.min_length_desired = 0.9 * length_threshold;  // If an element is too small, collaps
    
    cout << "Done "<<endl;

    Omega_h::vtk::Writer writer2("before", &mesh);
    writer2.write();
    
    //for (Int i = 0; i < 8; ++i) {
        //cout <<"Pass "<<i<<endl;
        auto coords = mesh.coords();
        Write<Real> warp_w(mesh.nverts() * dim);

        bool refine_needed = false;
        // Compute angles between all edge pairs
        Real max_angle,min_angle;
        max_angle = 0.0;min_angle = 180.0;
        Real max_length = 0.0;
        
        for (LO elem = 0; elem < mesh.nelems(); ++elem) {
            for (int j = 0; j < dim + 1; ++j) {
                for (int k = j + 1; k < dim + 1; ++k) {
                  
                    auto vj = elems2verts.ab2b[elem * (dim + 1) + j];
                    auto vk = elems2verts.ab2b[elem * (dim + 1) + k];
                    auto pj = get_vector<dim>(vert_coords, vj);
                    auto pk = get_vector<dim>(vert_coords, vk);
                    auto edge_vector = pk - pj;
                    auto edge_length = norm(edge_vector);
                    if (edge_length>max_length)
                      max_length = edge_length;
                    if (edge_length > length_threshold) {
                        refine_needed = true;
                        break;
                    }
                
                }
            }


            for (int j = 0; j < dim + 1; ++j) {
                for (int k = j + 1; k < dim + 1; ++k) {
                    for (int l = k + 1; l < dim + 1; ++l) {
                      
                        auto vj = elems2verts.ab2b[elem * (dim + 1) + j];
                        auto vk = elems2verts.ab2b[elem * (dim + 1) + k];
                        auto vl = elems2verts.ab2b[elem * (dim + 1) + l];

                        auto pj = get_vector<dim>(vert_coords, vj);
                        auto pk = get_vector<dim>(vert_coords, vk);
                        auto pl = get_vector<dim>(vert_coords, vl);

                        auto edge1 = normalize(pk - pj);
                        auto edge2 = normalize(pl - pj);

                        Real dot_product = dot(edge1, edge2);
                        Real angle = acos(clamp(dot_product, -1.0, 1.0)) * (180.0 / PI);
                        cout << "angle "<<angle<<endl;
                        if (angle>max_angle )angle=max_angle;
                        else if (angle<min_angle )angle=min_angle;
                        if (angle < angle_threshold || angle > (180.0 - angle_threshold)) {
                            cout << "ANGLE THRESHOLD!"<<angle_threshold <<" REFINE"<<endl;
                            refine_needed = true;
                            break;
                        }
                      
                    }
                    if (refine_needed) break;
                }
                if (refine_needed) break;
            }
            if (refine_needed) break;
        }
        cout << "Max edge length: "<<max_length<<endl;
        cout << "Initial angles Min: "<<min_angle<<", max: "<<max_angle<<endl;
        if (refine_needed) {
            adapt(&mesh, opts);
            cout << "adapt"<<endl;
        }
    //} //i to 8
    mesh.set_parting(OMEGA_H_ELEM_BASED);
}
///////////////////////////////// ANGLE

Omega_h::Reals compute_min_angles(Omega_h::Mesh& mesh) {
  auto coords = mesh.coords();
  auto elems2verts = mesh.ask_elem_verts();
  auto nelems = mesh.nelems();

  Omega_h::Write<Omega_h::Real> min_angles(nelems);

  Omega_h::parallel_for(nelems, OMEGA_H_LAMBDA(Omega_h::LO e) {
    Omega_h::Real min_angle = 180.0;

    // Get the vertices of the element (assuming tetrahedral elements)
    auto v0 = elems2verts[e * 4 + 0];
    auto v1 = elems2verts[e * 4 + 1];
    auto v2 = elems2verts[e * 4 + 2];
    auto v3 = elems2verts[e * 4 + 3];

    // Get vertex coordinates
    auto p0 = Omega_h::get_vector<3>(coords, v0);
    auto p1 = Omega_h::get_vector<3>(coords, v1);
    auto p2 = Omega_h::get_vector<3>(coords, v2);
    auto p3 = Omega_h::get_vector<3>(coords, v3);

    // Lambda to calculate the angle between two vectors
    auto angle_between = [](Omega_h::Vector<3> a, Omega_h::Vector<3> b) -> Omega_h::Real {
      Omega_h::Real dot = Omega_h::inner_product(a, b);
      Omega_h::Real norm_a = Omega_h::norm(a);
      Omega_h::Real norm_b = Omega_h::norm(b);
      Omega_h::Real cos_angle = Omega_h::clamp(dot / (norm_a * norm_b), -1.0, 1.0);
      return std::acos(cos_angle) * 180.0 / PI;
    };

    // Calculate angles between edge vectors
    Omega_h::Vector<3> vectors[6] = {
      p1 - p0, p2 - p0, p3 - p0,
      p2 - p1, p3 - p1,
      p3 - p2
    };

    for (int i = 0; i < 6; ++i) {
      for (int j = i + 1; j < 6; ++j) {
        Omega_h::Real angle = angle_between(vectors[i], vectors[j]);
        min_angle = Omega_h::min2(min_angle, angle);
      }
    }

    min_angles[e] = min_angle;
  });

  return Omega_h::Reals(min_angles);
}


void compute_angle_metric(Omega_h::Mesh& mesh){

  auto opts = Omega_h::AdaptOpts(&mesh);

  opts.max_length_desired = 1.5 ;  // If an element is too big, split it
  opts.min_length_desired = 0.2 ;  // If an element is too small, collaps
  
  opts.verbosity = Omega_h::EXTRA_STATS;
  //opts.min_quality_allowed = 1.0e-3;
  opts.should_refine = true;
  opts.should_coarsen = true;  // Changed to allow coarsening based on metric
  //opts.verbosity = Omega_h::SILENT;  // Suppress all non-critical outp
  
  // Step 1: Create a vertex-based metric array
  Omega_h::Write<Omega_h::Real> vertex_metric(mesh.nverts(), 1.0);

  // Step 2: Average the element-based metric to vertices
  auto elems2verts = mesh.ask_elem_verts();
  auto min_angles = compute_min_angles(mesh);

  Omega_h::parallel_for(mesh.nverts(), OMEGA_H_LAMBDA(Omega_h::LO v) {
      Omega_h::Real sum_metric = 0.0;
      int count = 0;

      for (Omega_h::LO e = 0; e < mesh.nelems(); ++e) {
          for (int j = 0; j < 4; ++j) {  // Tetrahedron has 4 vertices
              if (elems2verts[e * 4 + j] == v) {
                  // Use your angle-based metric calculation here
                  if (min_angles[e] < 40.0 || min_angles[e] > 140.0) {
                      sum_metric += 0.5; // Refine bad-angle elements
                  } else {
                      sum_metric += 1.0; // Coarsen good-angle elements
                  }
                  count++;
              }
          }
      }

      // Step 3: Assign the averaged metric
      vertex_metric[v] = (count > 0) ? (sum_metric / count) : 1.0;
  });

  // Step 4: Add the metric tag to vertices
  //mesh.add_tag<Omega_h::Real>(Omega_h::VERT, "metric", 1);
  mesh.set_tag(Omega_h::VERT, "metric", Omega_h::Reals(vertex_metric));
  
  //auto metrics = get_implied_isos(&mesh);
  //mesh.set_tag(Omega_h::VERT, "metric", metrics);  

  //Omega_h::Write<Omega_h::Real> uniform_metric(mesh.nverts(), 0.1);
  //mesh.add_tag(Omega_h::VERT, "metric", 1, Omega_h::Reals(uniform_metric));
  

  //bool remesh = false;
  //for (LO v=0;v<mesh.nelems();v++){
  //  if (vertex_metric[v]<0.5) remesh = true;
    
  //}
  // Adapt mesh
  //if (remesh)
    Omega_h::adapt(&mesh, opts);
  
}

void compute_angle_metric_selective(Omega_h::Mesh& mesh) {
  // Set up adapt options
  auto opts = Omega_h::AdaptOpts(&mesh);
  opts.max_length_desired = 1.5;
  opts.min_length_desired = 0.2;
  opts.should_refine = true;
  opts.should_coarsen = true;
  //opts.verbosity = Omega_h::EXTRA_STATS;
  opts.verbosity = Omega_h::SILENT;
  
  double orig_len = 0.15;

  // Step 1: Get baseline scalar metric using implied isos
  auto implied_metric = get_implied_isos(&mesh);  // scalar field, size = nverts()

  // Step 2: Compute min angles per element
  auto min_angles = compute_min_angles(mesh);

  // Step 3: Build modified vertex metrics using angle-based refinement
  auto elems2verts = mesh.ask_elem_verts();
  auto vertex_metric = Omega_h::Write<Omega_h::Real>(mesh.nverts());

  Omega_h::parallel_for(mesh.nverts(), OMEGA_H_LAMBDA(Omega_h::LO v) {
    Omega_h::Real modified_metric = implied_metric[v];  // Default: keep original

    // Loop over connected elements to check their min angle
    bool refine_this = false;
    for (int e = 0; e < mesh.nelems(); ++e) {
      for (int j = 0; j < 4; ++j) {
        if (elems2verts[e * 4 + j] == v) {
          if (min_angles[e] < 40.0 || min_angles[e] > 140.0) {
            refine_this = true;
          }
        }
      }
    }

    if (refine_this) {
      // Set refined metric: M = 1 / h^2, e.g., h = 0.2
      Omega_h::Real h_refined = 0.4 * orig_len;
      Omega_h::Real refined_metric = 1.0 / (h_refined * h_refined);

      // Take the more refined one (i.e., larger metric value = smaller h)
      //modified_metric = Omega_h::max(modified_metric, refined_metric);
      modified_metric = std::max(modified_metric, refined_metric);
    }

    vertex_metric[v] = modified_metric;
  }
  ); //VERTICES

  // Step 4: Assign updated metric to mesh
  mesh.set_tag(Omega_h::VERT, "metric", Omega_h::Reals(vertex_metric));

  auto met2 = get_implied_isos(&mesh);
  mesh.set_tag(Omega_h::VERT, "metric_imp", met2);  

  //auto quality = measure_element_quality(mesh)
  //mesh.add_tag("quality", omega_h.Region, 1, quality)

  // Step 5: Run mesh adaptation
  Omega_h::adapt(&mesh, opts);
}


void adapt_mesh_based_on_temperature(Mesh& mesh) {
  
    add_pointwise(&mesh);
    // Assume we have temperature values calculated somewhere
    // Here, we'll just generate a dummy temperature field for the example
    auto vert_coords = mesh.coords();
    auto elems2verts = mesh.ask_down(3, VERT);
        
    Write<Real> temperature_field(mesh.nverts(), 0.01);
    for (LO i = 0; i < mesh.nverts(); ++i) {
        auto pj = get_vector<3>(vert_coords, 0);
        auto pk = get_vector<3>(vert_coords, 1);
        auto edge_length = norm(pj);
        temperature_field[i] = 0.01; // Dummy temperature variation
    }

/*
      for (LO elem = 0; elem < mesh.nelems(); ++elem) {
        std::cout << "ELEMENT "<<elem<<std::endl; 
                auto vj = elems2verts.ab2b[elem * (3 + 1) + 0];
                auto vk = elems2verts.ab2b[elem * (3 + 1) + 1];
                auto pj = get_vector<3>(vert_coords, vj);
                auto pk = get_vector<3>(vert_coords, vk);
                auto edge_length = norm(pk - pj);
                temperature_field[vj] = edge_length; // Dummy temperature variation                   
      }//Elem
*/

    
    // Step 2: Calculate mesh density (refinement level) based on temperature gradient
    Write<Real> density_field(mesh.nverts(), 1.0);  // Initialize with a default value (coarse mesh)
    for (LO i = 1; i < mesh.nverts() - 1; ++i) {
        density_field[i] = temperature_field[i];
        std::cout << "MESH DENSTITY "<<density_field[i]<<std::endl;
        /*
        // Calculate the temperature gradient between neighboring vertices
        Real grad_temp = std::abs(temperature_field[i + 1] - temperature_field[i - 1]);
        // Increase density where the temperature gradient is large
        if (grad_temp > 0.1) {
            density_field[i] = 0.5;  // Refinement (smaller elements in regions with larger gradients)
        }
        */
    }

    // Step 4: Perform mesh adaptation
    AdaptOpts opts(&mesh);


    // Step 1: Set the pointwise data (temperature values at each vertex)
    mesh.add_tag(VERT, "pointwise", 1, Reals(temperature_field));
    
    // Step 3: Set the density tag (guide mesh adaptation)
    mesh.add_tag(VERT, "density", 1, Reals(density_field));
    // Transfer options for the adaptation
    //opts.xfer_opts.type_map["density"] = OMEGA_H_CONSERVE;  // Conserve density during adaptation
    opts.xfer_opts.type_map["density"] = OMEGA_H_LINEAR_INTERP;  // Interpolate pointwise data during adaptation

    opts.xfer_opts.type_map["pointwise"] = OMEGA_H_POINTWISE;  // Use pointwise data to guide adaptation
    
    
    //opts.xfer_opts.type_map["mesh_density"] = OMEGA_H_LINEAR_INTERP;  // Interpolate mesh density data
    opts.xfer_opts.integral_map["density"] = "mass";  // Set the integral map to "mass"    


  opts.xfer_opts.integral_diffuse_map["mass"] = VarCompareOpts::none();


    //opts.verbosity = Omega_h::AdaptOpts::EXTRA_STATS; // Optional: to get more information during remeshing


    // Set mesh density for the adaptation



    // Perform mesh adaptation based on the set density and temperature
    //adapt(&mesh, opts);
    // Apply the remeshing process

   // Omega_h::AmrOpts refine_opts(&mesh);

    // Set criteria for refinement (e.g., refine by element size)
    //refine_opts.criteria = Omega_h::AmrOpts::REFINE_BY_SIZE;
    
    //Omega_h::amr::refine(&mesh, refine_opts);
    //Real  desired_size = 0.8;
    
    //Omega_h::Write<Real> size_values(mesh.nelems(), 1.0);  // Example size values (can be based on density)
    //mesh.add_tag(Omega_h::CELL, "size", 1, size_values);  // Attach size field to mesh

    //opts.xfer_opts.type_map["size"] = OMEGA_H_LINEAR_INTERP;  // Ensure size field is interpolated
    opts.min_quality_allowed = 1.0e-3;  // Prevent bad-quality elements
    opts.should_refine = true;  // Allow refinement
    opts.should_coarsen = false;  // Allow coarsening
    //opts.max_length_desired = 1.2 * desired_size;  // If an element is too big, split it
    //opts.min_length_desired = 0.75 * desired_size;  // If an element is too small, collaps

//while (warp_to_limit(&mesh, opts)) {
    adapt(&mesh, opts);  // Adapt the mesh based on the defined metric (density, pointwise)

//}
    //postprocess_pointwise(&mesh);    
    std::cout << "New mesh verts : "<<mesh.nverts()<<std::endl;
    std::cout << "New mesh elems ; "<<mesh.nelems()<<std::endl;
    std::cout << "Mesh adaptation complete based on temperature gradient and mesh density." << std::endl;
}



static void run_2D_adapt(Omega_h::Library* lib) {
  auto w = lib->world();
  auto f = OMEGA_H_HYPERCUBE;
  auto m = Omega_h::build_box(w, f, 1.0, 1.0, 0.0, 2, 2, 0);
  Omega_h::vtk::FullWriter writer("out_amr_2D", &m);
  writer.write();
  // refine
  Omega_h::Write<Omega_h::Byte> refine_marks(m.nelems(), 1);
  auto xfer_opts = Omega_h::TransferOpts();
  Omega_h::amr::refine(&m, refine_marks, xfer_opts);
  writer.write();
  // de-refine
  Omega_h::Write<Omega_h::Byte> derefine_marks(m.nelems(), 0);
  derefine_marks.set(0, 1);
  Omega_h::amr::derefine(&m, derefine_marks, xfer_opts);
  writer.write();
}


void ReMesher::Generate_omegah(){
  
    
    
    double length_tres = 0.85;
    double ang_tres = 40.0;

  // Compute angle-based metric

    Omega_h::vtk::Writer writer2("before", &m_mesh);
    writer2.write();
    
  //compute_angle_metric(mesh);
  compute_angle_metric_selective(m_mesh);
  Omega_h::vtk::Writer writer3("out_amr_angle_3D", &m_mesh);

    writer3.write();
  


  
  m_x = new double [3*m_mesh.nverts()];
  m_elnod = new int [m_mesh.nelems() * 4]; //Flattened
  

  //cout << "CONVERTING MESH"<<endl;
  auto coords = m_mesh.coords();
     cout << "Setting conn"<<endl; 
  auto f = OMEGA_H_LAMBDA(LO vert) {
    auto x = get_vector<3>(coords, vert); //dim crashes
    //std::cout<< "VERT "<<vert<<std::endl;
  //for (int n = 0; n < mesh.nverts(); n++) {
    bool found = false;  // Flag to indicate whether the node is inside an element in the old mesh
    //std::cout<< "NODES "<<std::endl;
    //std::cout << m_mesh.coords()[vert]<<std::endl;
    
      // Get coordinates for the node in the new mesh
      std::array<double, 3> target_node = {x[0], x[1], x[2]}; // Now using 3D coordinates
      for (int d=0;d<3;d++)m_x[3*vert+d] = x[d];

    //n++;
  };//NODE LOOP
  parallel_for(m_mesh.nverts(), f); 

  auto elems2verts = m_mesh.ask_down(3, VERT);
        
  auto fe = OMEGA_H_LAMBDA(LO elem) {

    bool found = false;  // Flag to indicate whether the node is inside an element in the old mesh
    //std::cout<< "ELEM "<<std::endl;
    for (int ve=0;ve<4;ve++){
      auto v = elems2verts.ab2b[elem * 4 + ve];
      m_elnod[4*elem+ve] = v;
      //cout << v <<" ";
      }
    //cout <<endl;
  };//NODE LOOP  
  parallel_for(m_mesh.nelems(), fe); 

  
  
}


//USES DOMAIN TO INTERPOLATE VARS
//HOST MAP
//Args: mesh, dest field, original field
//THE ORIGINAL MESH IS m_dom->
template <int dim>
void ReMesher::MapNodalVector(Mesh& mesh, double *vfield, double *o_field) {
    // Loop over the target nodes in the new mesh
    auto coords = mesh.coords();

    
    auto f = OMEGA_H_LAMBDA(LO vert) {
      auto x = get_vector<3>(coords, vert);
      
      bool found_samenode = false;
      double tol = 1.0e-3;
      //SEARCH OVERALL NEW MESH NODES IF NOT A NEW NODE NEAR THE OLD NODE  
      for (int v = 0; v < m_dom->m_node_count; v++){
        //If new node dist <tol, map new node = old node
        std::array<double, 3> p0 = {m_dom->x[3 * v], m_dom->x[3 * v + 1], m_dom->x[3 * v + 2]};
        double distance = 0.0;
        for (int i = 0; i < 3; ++i) {
            distance += (x[i] - p0[i]) * (x[i] - p0[i]);
        }
        if (distance<tol){
          found_samenode = true;
          //cout << "FOUND " << vert << " SAME NODE "<<endl;
          for (int d=0;d<3;d++) vfield[3*vert+d] = o_field[3*v+d];
        }                
      }//node
      
      if (!found_samenode){
    //for (int n = 0; n < mesh.nverts(); n++) {
        bool found = false;  // Flag to indicate whether the node is inside an element in the old mesh
        //std::cout << mesh.coords()[n]<<std::endl;
        
        // Get coordinates for the node in the new mesh
        std::array<double, 3> target_node = {x[0], x[1], x[2]}; // Now using 3D coordinates
        
        // Loop over the elements in the old mesh (using *elnod to access connectivity and *node for coordinates)
        for (int i = 0; i < m_dom->m_elem_count; i++) {
            // Connectivity for the tetrahedral element (assumed to have 4 nodes per element in the old mesh)
            int n0 = m_dom->m_elnod[4*i];   // Node 0 in the element
            int n1 = m_dom->m_elnod[4*i+1]; // Node 1 in the element
            int n2 = m_dom->m_elnod[4*i+2]; // Node 2 in the element
            int n3 = m_dom->m_elnod[4*i+3]; // Node 3 in the element

            std::array<double, 3> p0 = {m_dom->x[3*n0], m_dom->x[3*n0+1], m_dom->x[3*n0+2]};
            std::array<double, 3> p1 = {m_dom->x[3*n1], m_dom->x[3*n1+1], m_dom->x[3*n1+2]};
            std::array<double, 3> p2 = {m_dom->x[3*n2], m_dom->x[3*n2+1], m_dom->x[3*n2+2]};
            std::array<double, 3> p3 = {m_dom->x[3*n3], m_dom->x[3*n3+1], m_dom->x[3*n3+2]};

            std::array<double, 4> lambdas = barycentric_coordinates(target_node, p0, p1, p2, p3);

            if (lambdas[0] >= -5.0e-2 && lambdas[1] >= -5.0e-2 && lambdas[2] >= -5.0e-2 && lambdas[3] >= -5.0e-2) { 
                //std::cout << "FOUND ELEMENT "<<i << " For node "<<vert<<std::endl;
                //double scalar[4];
                //for (int n=0;n<4;n++) scalar[n] = m_dom->pl_strain[i];

                //double interpolated_scalar = interpolate_scalar(target_node, p0, p1, p2, p3, scalar[0], scalar[1], scalar[2], scalar[3]);


                // Interpolate vector values for displacement (if needed)
                std::array<double, 3> disp[4];
                for (int n=0;n<4;n++)
                  for (int d=0;d<3;d++)
                    disp[n][d] = o_field[3*m_dom->m_elnod[4*i+n]+d];
                
                //cout << "Interp disp"<<endl;
                std::array<double, 3> interpolated_disp = interpolate_vector(target_node, p0, p1, p2, p3, disp[0], disp[1], disp[2],disp[3]);
                for (int d=0;d<3;d++) vfield[3*vert+d] = interpolated_disp[d];
                // Optionally, interpolate other scalar/vector fields for the new mesh node here

                //std::cout << "Node " << vert << " is inside element " << i << " of the old mesh." << std::endl;
                //std::cout << "Interpolated scalar: " << interpolated_scalar << std::endl;
                //std::cout << "Interpolated displacement: (" << interpolated_disp[0] << ", " << interpolated_disp[1] << ", " << interpolated_disp[2] << ")\n";

                found = true;
                break;  // Exit the element loop once the element is found
            }//lambdas
          }//elem
          if (!found) {
              std::cout << "Node " << vert << " is not inside any element of the old mesh." << std::endl;
          }
        }//found same node

      //n++;
    };//NODE LOOP
    parallel_for(mesh.nverts(), f);

}//MAP
  

template <int dim>
void ReMesher::MapElemVector(Mesh& mesh, double *vfield, double *o_field, int field_dim) {
    auto coords = mesh.coords();
    double *scalar = new double[mesh.nverts()];
    double *vector = new double[mesh.nverts() * 3];

    auto elems2verts = m_mesh.ask_down(3, VERT);
    double max_field_val = 0.;

    
    auto f = OMEGA_H_LAMBDA(LO elem) {
        bool found = false;
        std::array<double, 3> barycenter = {0.0, 0.0, 0.0};

        std::array<double, 3> barycenter_old_clos = {0.0, 0.0, 0.0};

        // Calculate barycenter of the current new element
        for (int en = 0; en < 4; en++) {
            auto v = elems2verts.ab2b[elem * 4 + en];
            auto x = get_vector<3>(coords, v);
            barycenter[0] += x[0];
            barycenter[1] += x[1];
            barycenter[2] += x[2];
        }
        barycenter[0] /= 4.0;
        barycenter[1] /= 4.0;
        barycenter[2] /= 4.0;

        // Search for the closest old element by distance
        double min_distance = std::numeric_limits<double>::max();
        int closest_elem = -1;

        for (int i = 0; i < m_dom->m_elem_count; i++) {
            int n0 = m_dom->m_elnod[4 * i];
            int n1 = m_dom->m_elnod[4 * i + 1];
            int n2 = m_dom->m_elnod[4 * i + 2];
            int n3 = m_dom->m_elnod[4 * i + 3];

            std::array<double, 3> p0 = {m_dom->x[3 * n0], m_dom->x[3 * n0 + 1], m_dom->x[3 * n0 + 2]};
            std::array<double, 3> p1 = {m_dom->x[3 * n1], m_dom->x[3 * n1 + 1], m_dom->x[3 * n1 + 2]};
            std::array<double, 3> p2 = {m_dom->x[3 * n2], m_dom->x[3 * n2 + 1], m_dom->x[3 * n2 + 2]};
            std::array<double, 3> p3 = {m_dom->x[3 * n3], m_dom->x[3 * n3 + 1], m_dom->x[3 * n3 + 2]};

            // Calculate the barycenter of the old element
            std::array<double, 3> old_barycenter = {
                (p0[0] + p1[0] + p2[0] + p3[0]) / 4.0,
                (p0[1] + p1[1] + p2[1] + p3[1]) / 4.0,
                (p0[2] + p1[2] + p2[2] + p3[2]) / 4.0
            };

            double distance = 
            //std::sqrt(
                std::pow(barycenter[0] - old_barycenter[0], 2) +
                std::pow(barycenter[1] - old_barycenter[1], 2) +
                std::pow(barycenter[2] - old_barycenter[2], 2)
            ;
            //);

            if (distance < min_distance) {
                min_distance = distance;
                closest_elem = i;
                found = true;
                barycenter_old_clos = old_barycenter;
            }
        }//elem

        //cout << "Closest element new - old "<< elem<<" - "<<closest_elem<<endl;
        //cout << "Baricenter old "<<barycenter_old_clos[0]<<" "<<barycenter_old_clos[1]<<" "<<barycenter_old_clos[2]<<endl;
        //cout << "Baricenter new "<<barycenter[0]<<" "<<barycenter[1]<<" "<<barycenter[2]<<endl;
        //cout << "Min dist "<<sqrt(min_distance)<<endl;
        for (int d=0;d<field_dim;d++) {
          vfield[elem*field_dim+d] = o_field[closest_elem*field_dim+d];
          //cout << vfield[elem*field_dim+d]<< " ";
        }
        //cout <<endl;
        //if (vfield[elem*field_dim] > max_field_val)
        //  max_field_val = vfield[elem*field_dim];
       
        if (found) {
            //std::cout << "Mapped element " << elem << " to old element " << closest_elem << std::endl;
        } else {
            std::cout << "ERROR: No matching element found for element " << elem << std::endl;
        }
    }; //LAMBDA

    parallel_for(mesh.nelems(), f);
    
    //cout << "MAX FIELD VALUE: "<<max_field_val<<endl;
}


template <int dim>
void ReMesher::MapElemVectors() {
  
    auto coords = m_mesh.coords();
    double *scalar = new double[m_mesh.nverts()];
    double *vector = new double[m_mesh.nverts() * 3];

    auto elems2verts = m_mesh.ask_down(3, VERT);
    double max_field_val = 0.;

    
    auto f = OMEGA_H_LAMBDA(LO elem) {
        bool found = false;
        std::array<double, 3> barycenter = {0.0, 0.0, 0.0};

        std::array<double, 3> barycenter_old_clos = {0.0, 0.0, 0.0};

        // Calculate barycenter of the current new element
        for (int en = 0; en < 4; en++) {
            auto v = elems2verts.ab2b[elem * 4 + en];
            auto x = get_vector<3>(coords, v);
            barycenter[0] += x[0];
            barycenter[1] += x[1];
            barycenter[2] += x[2];
        }
        barycenter[0] /= 4.0;
        barycenter[1] /= 4.0;
        barycenter[2] /= 4.0;

        // Search for the closest old element by distance
        double min_distance = std::numeric_limits<double>::max();
        int closest_elem = -1;

        for (int i = 0; i < m_dom->m_elem_count; i++) {
            int n0 = m_dom->m_elnod[4 * i];
            int n1 = m_dom->m_elnod[4 * i + 1];
            int n2 = m_dom->m_elnod[4 * i + 2];
            int n3 = m_dom->m_elnod[4 * i + 3];

            std::array<double, 3> p0 = {m_dom->x[3 * n0], m_dom->x[3 * n0 + 1], m_dom->x[3 * n0 + 2]};
            std::array<double, 3> p1 = {m_dom->x[3 * n1], m_dom->x[3 * n1 + 1], m_dom->x[3 * n1 + 2]};
            std::array<double, 3> p2 = {m_dom->x[3 * n2], m_dom->x[3 * n2 + 1], m_dom->x[3 * n2 + 2]};
            std::array<double, 3> p3 = {m_dom->x[3 * n3], m_dom->x[3 * n3 + 1], m_dom->x[3 * n3 + 2]};

            // Calculate the barycenter of the old element
            std::array<double, 3> old_barycenter = {
                (p0[0] + p1[0] + p2[0] + p3[0]) / 4.0,
                (p0[1] + p1[1] + p2[1] + p3[1]) / 4.0,
                (p0[2] + p1[2] + p2[2] + p3[2]) / 4.0
            };

            double distance = 
            //std::sqrt(
                std::pow(barycenter[0] - old_barycenter[0], 2) +
                std::pow(barycenter[1] - old_barycenter[1], 2) +
                std::pow(barycenter[2] - old_barycenter[2], 2)
            ;
            //);

            if (distance < min_distance) {
                min_distance = distance;
                closest_elem = i;
                found = true;
                barycenter_old_clos = old_barycenter;
            }
        }//elem

/*
        for (int d=0;d<field_dim;d++) {
          vfield[elem*field_dim+d] = o_field[closest_elem*field_dim+d];

        }
*/
       
    }; //LAMBDA

    parallel_for(m_mesh.nelems(), f);
    

}

template <int dim, typename T>
void ReMesher::MapElemPtrVector(Mesh& mesh, T* vfield, T* o_field) {
    auto coords = mesh.coords();
    T* scalar = new T[mesh.nverts()];
    T* vector = new T[mesh.nverts() * 3];

    auto elems2verts = m_mesh.ask_down(3, VERT);
    T max_field_val = static_cast<T>(0);

    auto f = OMEGA_H_LAMBDA(LO elem) {
        bool found = false;
        std::array<double, 3> barycenter = {0.0, 0.0, 0.0};
        std::array<double, 3> barycenter_old_clos = {0.0, 0.0, 0.0};

        // Calculate barycenter of the current new element
        for (int en = 0; en < 4; en++) {
            auto v = elems2verts.ab2b[elem * 4 + en];
            auto x = get_vector<3>(coords, v);
            barycenter[0] += x[0];
            barycenter[1] += x[1];
            barycenter[2] += x[2];
        }
        barycenter[0] /= 4.0;
        barycenter[1] /= 4.0;
        barycenter[2] /= 4.0;

        // Search for the closest old element by distance
        double min_distance = std::numeric_limits<double>::max();
        int closest_elem = -1;

        for (int i = 0; i < m_dom->m_elem_count; i++) {
            int n0 = m_dom->m_elnod[4 * i];
            int n1 = m_dom->m_elnod[4 * i + 1];
            int n2 = m_dom->m_elnod[4 * i + 2];
            int n3 = m_dom->m_elnod[4 * i + 3];

            std::array<double, 3> p0 = {m_dom->x[3 * n0], m_dom->x[3 * n0 + 1], m_dom->x[3 * n0 + 2]};
            std::array<double, 3> p1 = {m_dom->x[3 * n1], m_dom->x[3 * n1 + 1], m_dom->x[3 * n1 + 2]};
            std::array<double, 3> p2 = {m_dom->x[3 * n2], m_dom->x[3 * n2 + 1], m_dom->x[3 * n2 + 2]};
            std::array<double, 3> p3 = {m_dom->x[3 * n3], m_dom->x[3 * n3 + 1], m_dom->x[3 * n3 + 2]};

            // Calculate the barycenter of the old element
            std::array<double, 3> old_barycenter = {
                (p0[0] + p1[0] + p2[0] + p3[0]) / 4.0,
                (p0[1] + p1[1] + p2[1] + p3[1]) / 4.0,
                (p0[2] + p1[2] + p2[2] + p3[2]) / 4.0
            };

            double distance = 
                std::pow(barycenter[0] - old_barycenter[0], 2) +
                std::pow(barycenter[1] - old_barycenter[1], 2) +
                std::pow(barycenter[2] - old_barycenter[2], 2);

            if (distance < min_distance) {
                min_distance = distance;
                closest_elem = i;
                found = true;
                barycenter_old_clos = old_barycenter;
            }
        }


            vfield[elem] = o_field[closest_elem];

        if (!found) {
            std::cout << "ERROR: No matching element found for element " << elem << std::endl;
        }
    }; // LAMBDA

    parallel_for(mesh.nelems(), f);
}



// Type aliases for clarity
using Vec3 = std::array<double, 3>;
using Tetra = std::array<int, 4>;

// Compute volume of tetrahedron given its 4 coordinates
double TetraVolume(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    Vec3 ab = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    Vec3 ac = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    Vec3 ad = {d[0] - a[0], d[1] - a[1], d[2] - a[2]};

    double vol = (ab[0] * (ac[1] * ad[2] - ac[2] * ad[1]) -
                  ab[1] * (ac[0] * ad[2] - ac[2] * ad[0]) +
                  ab[2] * (ac[0] * ad[1] - ac[1] * ad[0])) / 6.0;
    return std::abs(vol);
}
template <int dim>
void ReMesher::ProjectElemToNodes(Mesh& mesh,  double* elemvals, double* nodal_vals, int field_dim) {
    auto coords = mesh.coords();
    auto elems2verts = mesh.ask_down(3, 0);
    int nelems = mesh.nelems();
    int nverts = mesh.nverts();

    std::vector<double> nodal_weights(nverts, 0.0);
    std::fill(nodal_vals, nodal_vals + field_dim * nverts, 0.0);

    for (int e = 0; e < nelems; ++e) {
        std::array<int, 4> verts;
        for (int i = 0; i < 4; ++i)
            verts[i] = elems2verts.ab2b[e * 4 + i];

        std::array<std::array<double, dim>, 4> x;
        for (int i = 0; i < 4; ++i)
            for (int d = 0; d < dim; ++d)
                x[i][d] = coords[dim * verts[i] + d];

        double vol = TetraVolume(x[0], x[1], x[2], x[3]); // this must support dim=3

        for (int j = 0; j < 4; ++j) {
            int node = verts[j];
            for (int fd = 0; fd < field_dim; ++fd)
                nodal_vals[node * field_dim + fd] += vol * elemvals[e * field_dim + fd];
            nodal_weights[node] += vol;
        }
    }

    for (int i = 0; i < nverts; ++i) {
        double w = nodal_weights[i];
        if (w < 1e-12) continue;
        for (int fd = 0; fd < field_dim; ++fd)
            nodal_vals[i * field_dim + fd] /= w;
    }
}

//mesh, origin(nodal), dest(elem)
template <int dim>
void ReMesher::ProjectNodesToElem(Mesh& mesh,  double* nodal_vals, double* elemvals, int field_dim) {
    auto elems2verts = mesh.ask_down(3, 0);
    int nelems = mesh.nelems();

    for (int e = 0; e < nelems; ++e) {
        std::array<int, 4> verts;
        for (int i = 0; i < 4; ++i)
            verts[i] = elems2verts.ab2b[e * 4 + i];

        for (int fd = 0; fd < field_dim; ++fd) {
            double sum = 0.0;
            for (int j = 0; j < 4; ++j)
                sum += nodal_vals[verts[j] * field_dim + fd];
            elemvals[e * field_dim + fd] = sum / 4.0;
        }
    }
}

/// 

template <int dim>
void ReMesher::HybridProjectionElemToElem(Mesh& mesh, double* new_elemvals,  double* old_elemvals, int field_dim) {
    int nverts = mesh.nverts();
    int nelems = mesh.nelems();

    // Step 1: Project old element values to old nodal values using OLD mesh (m_dom->x)
    std::vector<double> nodal_vals_old(3*m_old_mesh.nverts() * field_dim, 0.0);
    //double *nodal_vals_old = new double [3*m_old_mesh.nverts() * field_dim];
    //HERE m_old_mesh is the same mesh than m_dom->x
    cout << "Elem to nodes, old vert count "<<m_old_mesh.nverts()<<"field dim "<<field_dim<<endl;
    //map to nodal old data
    ProjectElemToNodes<dim>(m_old_mesh, old_elemvals, nodal_vals_old.data(), field_dim);

    //ProjectElemToNodes<dim>(m_old_mesh, old_elemvals, nodal_vals_old, field_dim);
    
    cout << "mapping nodal to neew vert count "<< nverts <<endl;
    // Step 2: Interpolate old nodal values to new mesh  (mesh arg )nodes
    std::vector<double> nodal_vals_new(3*nverts * field_dim, 0.0);
    //double *nodal_vals_new = new double [3*nverts * field_dim];
    MapNodalVector<dim>(mesh,nodal_vals_new.data(), nodal_vals_old.data()/*, field_dim*/);  // You already implemented this
    //MapNodalVector<dim>(mesh,nodal_vals_new, nodal_vals_old/*, field_dim*/);  // You already implemented this
    
    cout << "projecting to elem "<<endl;
    // Step 3: Project new nodal values to new element values using NEW mesh
    //ProjectNodesToElem<dim>(mesh, nodal_vals_new, new_elemvals, field_dim);
    ProjectNodesToElem<dim>(mesh, nodal_vals_new.data(), new_elemvals, field_dim);
    cout << "done "<<endl;
    
    //delete[] nodal_vals_new;
    //delete[] nodal_vals_old; 
}

  
};