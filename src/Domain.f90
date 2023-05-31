
module Domain
use NodeData
use ElementData
use ModPrecision, only : fp_kind
!use Neighbor 

implicit none 

integer :: Dim, node_count, elem_count, Nproc, dof 
type(Node)	::nod
type(Element)	::elem

real(fp_kind), dimension(:,:), Allocatable :: mat_C !TODO: CHANGE TO SEVERAL MATERIALS
real(fp_kind), dimension(:,:), Allocatable :: kglob, uglob, m_glob


!THESE ARE VECTORS, NOT MATRICES (AND ARE NOT MULTIPLIED)
real(fp_kind), dimension(:,:), Allocatable :: rint_glob !Accelerations and internal forces
integer :: nodxelem !TODO: SET TO ELEMEENT VAR

real(fp_kind), dimension(3):: dommax, dommin

real(fp_kind)::mat_G, mat_E!TODO: change to material
real(fp_kind):: time
logical :: calc_m
contains 

  subroutine DomInit(proc)
    integer, intent(in)::proc
    nproc = proc
    Dim = 3
    DomMax (:) = -100000000000.0;
    DomMin (:) = 100000000000.0;
  end subroutine DomInit
  
  !After initialization of nodes and elements
  subroutine AllocateDomain()
    dof = node_count*dim
    
    allocate (kglob(node_count*dim,node_count*dim))
    allocate (uglob(node_count*dim,1))
    allocate (m_glob(node_count*dim,node_count*dim))
    
    allocate (rint_glob(node_count,dim))
  end subroutine AllocateDomain
  
  subroutine AllocateNodes(pt_count)
    integer, intent(in):: pt_count
    
    node_count = pt_count
    !!!GENERAL 
    allocate (nod%x(node_count,dim))
    ! allocate (nod%rho(node_count))
    ! allocate (nod%drhodt(node_count))
    ! allocate (nod%h(node_count))
    ! allocate (nod%m(node_count))
    allocate (nod%id(node_count))
    
    
    
    !! THERMAL
    ! allocate (nod%cp_t(node_count))
    ! allocate (nod%t(node_count))
    ! allocate (nod%k_t(node_count))
    
    !MECHANICAL PROPERTIES
    !if (solver_type==1) then 
    allocate (nod%u(node_count,dim))
    allocate (nod%v(node_count,dim))
    allocate (nod%a(node_count,dim))
    allocate (nod%disp(node_count,dim))
    
    ! allocate (nod%sigma(node_count,3,3))
    ! allocate (nod%str_rate(node_count,3,3))
    ! allocate (nod%rot_rate(node_count,3,3))
    ! allocate (nod%shear_stress(node_count,3,3))
    ! allocate (nod%strain(node_count))
    ! allocate (nod%pressure(node_count))
    ! allocate (nod%cs(node_count))
    
    ! allocate (nod%sigma_eq(node_count))
    
    ! allocate (nod%rho_0(node_count))
    !!!!! BOUNDARY CONDITIONS
    allocate (nod%is_bcv(node_count,dim))
    allocate (nod%is_fix(node_count,dim))
    allocate (nod%bcv(node_count,dim))
    !end if  
  end subroutine
  
  subroutine AllocateElements(el_count, gp) !gauss point count
    integer, intent(in):: el_count, gp
    
    print *, "Creating ", el_count, " elements"
    elem_count = el_count
    allocate (elem%elnod(el_count,nodxelem))
    allocate (elem%gausspc(el_count))
    allocate (elem%dof(el_count,dim*nodxelem))
    allocate (elem%vol(el_count))
    allocate (elem%x2(el_count,nodxelem,dim))
    allocate (elem%jacob(el_count,gp,dim,dim))
    allocate (elem%detj(el_count,gp))
    allocate (elem%sigma_eq(el_count,gp)) !But is constant??
    allocate (elem%dHxy(el_count,gp,dim,nodxelem))
    allocate (elem%tau(el_count,gp,dim*dim,dim*dim))

    allocate (elem%matKl(el_count,dim*nodxelem,dim*nodxelem))
    allocate (elem%matKnl(el_count,dim*nodxelem,dim*nodxelem))

    allocate (elem%uele (el_count,dim*nodxelem,1)) 
    
    allocate (elem%matm(el_count,dim*nodxelem,dim*nodxelem)) !Mass matrix
    allocate (elem%math(el_count,gp,dim,dim*nodxelem)) !Mass matrix
    
    if (Dim .eq. 2) then
      allocate (elem%bl (el_count,gp,3,dim*nodxelem))
      allocate (elem%bnl(el_count,gp, 4,dim*nodxelem))
      allocate (elem%strain(el_count,gp, 4,1))
    else 
      allocate (elem%bl (el_count,gp,6,dim*nodxelem))
      allocate (elem%strain(el_count,gp, 6,1))
    end if 
    
    elem%gausspc(:) = 1
    allocate (elem%rho(el_count)) !AT FIRST ONLY ONE POINT

  end subroutine

  !!!!! NODE DISTRIBUTION ARE LIKE IN FLANAGAN (1981), GOUDREAU, AND BENSON (1992)
  subroutine AddBoxLength(tag, V, Lx, Ly, Lz, r, Density,  h)			
    integer, intent(in):: tag
    !real(fp_kind), intent(in), allocatable :: V
    real(fp_kind), dimension(1:3), intent(in)  :: V ! input
    real(fp_kind), intent(in):: r, Lx, Ly, Lz, Density, h  
    real(fp_kind), dimension (1:3) :: Xp
    integer :: i, j, k, p, ex, ey, ez, nnodz
      
    integer, dimension(1:3) :: nel 
    
    nel(1) = nint(Lx/(2.0*r)) 
    nel(2) = nint(Ly/(2.0*r)) 
    if (Dim .eq. 2) then
      nel(3) = 0
      nodxelem = 4
    else
      nel(3) = nint(Lz/(2.0*r)) 
      nodxelem = 8
    end if
    
    Xp(3) = V(3) 
    

    write (*,*) "Creating Mesh ...", "Elements ", nel(1), ", ",nel(2)
    
    call AllocateNodes((nel(1) +1)* (nel(2)+1) * (nel(3)+1))
    
    write (*,*) "Box Node count ", node_count
    
    
    write (*,*) "xp ", Xp(:)    
    
    if (dim .eq. 2) then
    !write(*,*) "Box Particle Count is ", node_count
    p = 1
    !do while (Xp(3) <= (V(3)+Lz))
      j = 1;         Xp(2) = V(2)
      do while (j <= (nel(2) +1))
        i = 1
        Xp(1) = V(1)
        do while (i <= (nel(1) +1))
          nod%x(p,:) = Xp(:)
          print *,"node ",p , "X: ",Xp(:)
          p = p + 1
          Xp(1) = Xp(1) + 2 * r
          i = i +1
        end do
        Xp(2) = Xp(2) + 2 * r
        j = j +1
      end do 
      Xp(3) = Xp(3) + 2 * r
    !end do
    
    else 
      p = 1
      k = 1; Xp(3) = V(3)
      do while (k <= (nel(3) +1))
        j = 1;         Xp(2) = V(2)
        do while (j <= (nel(2) +1))
          i = 1
          Xp(1) = V(1)
          do while (i <= (nel(1) +1))
            nod%x(p,:) = Xp(:)
            print *,"node ",p , "X: ",Xp(:)
            p = p + 1
            Xp(1) = Xp(1) + 2 * r
            i = i +1
          end do
          Xp(2) = Xp(2) + 2 * r
          j = j +1
        end do 
        Xp(3) = Xp(3) + 2 * r
        k = k + 1
      end do    
    end if
    
    !! ALLOCATE ELEMENTS
    !! DIMENSION = 2
    if (dim .eq. 2) then
      call AllocateElements(nel(1) * nel(2),4)
    else 
      call AllocateElements(nel(1) * nel(2)*nel(3),4)
    end if
    
    if (dim .eq. 2) then
      ey = 0
      i = 1
      do while ( ey < nel(2))
          ex = 0
          do while (ex < nel(1)) 
              elem%elnod(i,:)=[(nel(1)+1)*(ey+1)+ex+2,(nel(1)+1)*(ey+1)+ex+1,(nel(1)+1)*ey + ex+1,(nel(1)+1)*ey + ex+2]         
              print *, "Element ", i , "Elnod", elem%elnod(i,:) 
              i=i+1
            ex = ex + 1
          end do
        ey = ey + 1
      end do  
    else 
      ez = 0
      i = 1
      nnodz = (nel(3)+1)*(nel(3)+1)
      do while ( ez < nel(3))
        ey = 0    
        do while ( ey < nel(2))
            ex = 0
            do while (ex < nel(1)) 
                !elem%elnod(i,:)=[(nel(1)+1)*(ey+1)+ex+2,(nel(1)+1)*(ey+1)+ex+1,(nel(1)+1)*ey + ex+1,(nel(1)+1)*ey + ex+2]       
                elem%elnod(i,:) = [ nnodz*ez + (nel(1)+1)*ey + ex+1,nnodz*ez + (nel(1)+1)*ey + ex+2, &
                                    nnodz*ez + (nel(1)+1)*(ey+1)+ex+2,nnodz*ez + (nel(1)+1)*(ey+1)+ex+1, &
                                    nnodz*(ez + 1) + (nel(1)+1)*ey + ex+1,nnodz*(ez + 1) + (nel(1)+1)*ey + ex+2, &
                                    nnodz*(ez + 1) + (nel(1)+1)*(ey+1)+ex+2,nnodz*(ez + 1)+ (nel(1)+1)*(ey+1)+ex+1]
                print *, "Element ", i , "Elnod", elem%elnod(i,:) 
                i=i+1
              ex = ex + 1
            end do
          ey = ey + 1
        end do 
        ez = ez + 1
      end do !el z
    end if !dim
    
    call AllocateDomain()
    i = 1
    do while ( i <= node_count)
      nod%is_bcv(i,:) = .false.
      i = i + 1
    end do
  
    ! nod%m(:)   = Density * Lx * Ly * Lz / node_count
    ! nod%rho(:)   = Density
    ! nod%rho_0(:) = Density
    !print *, "Particle mass ", nod%m(2)
    
    !nod%id(:) = tag
    
    
  end subroutine AddBoxLength
  
  
!THIS IS DONE IN ORDER TO USE THE SAME ASSEMBLY SUBROUTINE FOR 2D AND 3D
subroutine set_edof_from_elnod()
  integer :: e,dof,d,n

  do e=1,elem_count
    do n=1,nodxelem
      do d=1,dim

        dof = dim * (elem%elnod(e,n) - 1 ) + d
        !print *, "elem ", e, " dof loc", dim*(n-1)+d, "glob ", dof
        elem%dof (e,dim*(n-1)+d) = dof
      end do
    end do
    print *, "elem ", e, " dof ", elem%dof(e,:)
  end do

end subroutine

End Module Domain 
