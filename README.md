# EuLaFEM
Eulerian Lagrange FEM



CUDACXX=/usr/local/cuda-12.3/bin/nvcc cmake ../WeldFormFEM -DBUILD_GPU=ON

To update libraries (LSDynaReader and Math)

git submodule update --init --recursive



Link here #https://arnon.dk/matching-sm-architectures-arch-and-gencode-for-various-nvidia-cards/ to see different architectures. 


Cuurrently working axisymmetric with hourglass for area weight in F90 version

  reduced_int = .True.
  call AddBoxLength(0, V, Lx, Ly, 1.0d0, r, rho, h,reduced_int)

  !! AFTER ADD BOXLEN
  axisymm_vol_weight = .false.
  bind_dom_type = 3 !!!AXISYMM, AFTER CREATING BOX!

  
  elem%sigma_y(:,:) = 300.0e6
  
  do i=1,node_count
  print *, "NODE ELEMENTS "
    print *,"i count ", i , nod%elxnod(i),nod%nodel(i,:)
  end do

  nod%is_fix(:,3) = .true.
 
 
Then in calc elem forces:

'''
if Area weight
#-------------

              fa = 0.25d0/elem%radius(e,gp) * elem%detJ(e,gp) !!! THEN IS WEIGHTED BY 4 in case of gauss point =1
              !!! AREA WEIGHTED, BENSON EQN 2.4.3.2
              !!! 2.4.3.2 remains sig * Area/(4 r0), which is (4detJ)/(4r0) = detJ /r0
              !!! LATER IS MULTIPLIED BY WEIGHT WICH GIVES THE AREA

              elem%f_int(e,n,1) = elem%f_int(e,n,1) + elem%dHxy_detJ(e,gp,2,n) * elem%sigma (e,gp, 1,2) - &
                                                     (elem%sigma (e,gp, 1,1) - elem%sigma (e,gp, 3,3) ) * fa
                                                     
              elem%f_int(e,n,2) = elem%f_int(e,n,2) + elem%dHxy_detJ(e,gp,1,n) * elem%sigma (e,gp, 1,2) - &
                                                     elem%sigma (e,gp, 1,2) * fa          
              ! print *, "fa ", elem%dHxy_detJ(e,gp,1,n) * elem%sigma (e,gp, 1,2)
              ! print *, "term 2 ", elem%sigma (e,gp, 1,2) * fa    
              
'''