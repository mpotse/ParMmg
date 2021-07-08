/* =============================================================================
**  This file is part of the parmmg software package for parallel tetrahedral
**  mesh modification.
**  Copyright (c) Bx INP/Inria/UBordeaux, 2017-
**
**  parmmg is free software: you can redistribute it and/or modify it
**  under the terms of the GNU Lesser General Public License as published
**  by the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  parmmg is distributed in the hope that it will be useful, but WITHOUT
**  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
**  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
**  License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License and of the GNU General Public License along with parmmg (in
**  files COPYING.LESSER and COPYING). If not, see
**  <http://www.gnu.org/licenses/>. Please read their terms carefully and
**  use this copy of the parmmg distribution only if you accept them.
** =============================================================================
*/

/**
 * \file inout_pmmg.c
 * \brief io for the parmmg software
 * \author Algiane Froehly (InriaSoft)
 * \version 1
 * \date 07 2018
 * \copyright GNU Lesser General Public License.
 *
 * input/outputs for parmmg.
 *
 */

#include "parmmg.h"
#include "hdf5.h"

/**
 * \param n integer for which we want to know the number of digits
 *
 * \return the number of digits of n.
 *
 */
static inline
int PMMG_count_digits(int n) {

  int count = 0;
  while (n != 0) {
    n /= 10;
    ++count;
  }

  return count;
}

/**
 * \param parmesh pointer toward the parmesh structure.
 * \param inm pointer to the mesh file.
 * \param bin binary (1) or ascii (0) file.
 * \param iswp perform byte swapping (1) or not (0).
 * \param pos position of the communicators in the binary file.
 * \param ncomm number of communicators to read.
 * \param nitem_comm pointer to the nb of items in each communicator.
 * \param color pointer to the color of each communicator.
 * \param idx_loc pointer to the local indices of entities in each communicator.
 * \param idx_glo pointer to the global indices of entities in each communicator.
 *
 * \return 0 if fail, 1 otherwise
 *
 * Load parallel communicator in Medit format (only one group per process is
 * allowed).
 *
 */
int PMMG_loadCommunicator( PMMG_pParMesh parmesh,FILE *inm,int bin,int iswp,
                           int pos,int ncomm,int *nitem_comm,int *color,
                           int **idx_loc,int **idx_glo ) {
  char chaine[MMG5_FILESTR_LGTH],strskip[MMG5_FILESTR_LGTH];
  int binch,bpos;
  int *inxt;
  int ntot,k,idxl,idxg,icomm,i;

  PMMG_CALLOC(parmesh,inxt,ncomm,int,"inxt",return 0);

  rewind(inm);
  fseek(inm,pos,SEEK_SET);
  /* Read color and nb of items */
  ntot = 0;
  if(!bin) {
    for( icomm = 0; icomm < ncomm; icomm++ ) {
      MMG_FSCANF(inm,"%d %d",&color[icomm],&nitem_comm[icomm]);
      ntot += nitem_comm[icomm];
    }
  }
  else {
    for( icomm = 0; icomm < ncomm; icomm++ ) {
      MMG_FREAD(&k,MMG5_SW,1,inm);
      if(iswp) k=MMG5_swapbin(k);
      color[icomm] = k;
      MMG_FREAD(&k,MMG5_SW,1,inm);
      if(iswp) k=MMG5_swapbin(k);
      nitem_comm[icomm] = k;
      ntot += nitem_comm[icomm];
    }
  }
  /* Allocate indices arrays */
  for( icomm = 0; icomm < ncomm; icomm++ ) {
    PMMG_CALLOC(parmesh,idx_loc[icomm],nitem_comm[icomm],int,
                "idx_loc",return 0);
    PMMG_CALLOC(parmesh,idx_glo[icomm],nitem_comm[icomm],int,
                "idx_glo",return 0);
  }

  rewind(inm);
  if (!bin) {
    strcpy(chaine,"D");
    while(fscanf(inm,"%127s",&chaine[0])!=EOF && strncmp(chaine,"End",strlen("End")) ) {
      if ( chaine[0] == '#' ) {
        fgets(strskip,MMG5_FILESTR_LGTH,inm);
        continue;
      }
      if( parmesh->info.API_mode == PMMG_APIDISTRIB_faces ) {
        if(!strncmp(chaine,"ParallelCommunicatorTriangles",strlen("ParallelCommunicatorTriangles"))) {
          pos = ftell(inm);
          break;
        }
      } else if( parmesh->info.API_mode == PMMG_APIDISTRIB_nodes ) {
         if(!strncmp(chaine,"ParallelCommunicatorVertices",strlen("ParallelCommunicatorVertices"))) {
          pos = ftell(inm);
          break;
        }
      }
    }
  } else { //binary file
    while(fread(&binch,MMG5_SW,1,inm)!=0 && binch!=54 ) {
      if(iswp) binch=MMG5_swapbin(binch);
      if(binch==54) break;
      if(!ncomm && binch==72) { // ParallelCommunicatorTriangles
        MMG_FREAD(&bpos,MMG5_SW,1,inm); //NulPos
        if(iswp) bpos=MMG5_swapbin(bpos);
        pos = ftell(inm);
        break; // if parallel triangles are found, ignore parallel nodes
      } else if(!ncomm && binch==73) { // ParallelCommunicatorVertices
        MMG_FREAD(&bpos,MMG5_SW,1,inm); //NulPos
        if(iswp) bpos=MMG5_swapbin(bpos);
        pos = ftell(inm);
        rewind(inm);
        fseek(inm,bpos,SEEK_SET);
        continue;
      } else {
        MMG_FREAD(&bpos,MMG5_SW,1,inm); //NulPos
        if(iswp) bpos=MMG5_swapbin(bpos);
        rewind(inm);
        fseek(inm,bpos,SEEK_SET);
      }
    }
  }


  /* Read indices */
  if(!bin) {
    for( i = 0; i < ntot; i++ ) {
      MMG_FSCANF(inm,"%d %d %d",&idxl,&idxg,&icomm);
      idx_loc[icomm][inxt[icomm]] = idxl;
      idx_glo[icomm][inxt[icomm]] = idxg;
      inxt[icomm]++;
    }
  } else {
    for( i = 0; i < ntot; i++ ) {
      MMG_FREAD(&k,MMG5_SW,1,inm);
      if(iswp) k=MMG5_swapbin(k);
      idxl = k;
      MMG_FREAD(&k,MMG5_SW,1,inm);
      if(iswp) k=MMG5_swapbin(k);
      idxg = k;
      MMG_FREAD(&k,MMG5_SW,1,inm);
      if(iswp) k=MMG5_swapbin(k);
      icomm = k;
      idx_loc[icomm][inxt[icomm]] = idxl;
      idx_glo[icomm][inxt[icomm]] = idxg;
      inxt[icomm]++;
    }
  }

  PMMG_DEL_MEM(parmesh,inxt,int,"inxt");
  return 1;
}

/**
 * \param parmesh pointer toward the parmesh structure.
 * \param filename name of the file to load the mesh from.
 *
 * \return 0 if fail, 1 otherwise
 *
 * Load parallel communicators in Medit format (only one group per process is
 * allowed).
 *
 */
int PMMG_loadCommunicators( PMMG_pParMesh parmesh,const char *filename ) {
  MMG5_pMesh  mesh;
  int         meshver;
  int         API_mode,icomm,ier;
  int         ncomm,*nitem_comm,*color;
  int         **idx_loc,**idx_glo;
  FILE        *inm;
  int         bin;
  long        pos;
  int         iswp,k;
  int         binch,bpos;
  char        chaine[MMG5_FILESTR_LGTH],strskip[MMG5_FILESTR_LGTH];

  assert( parmesh->ngrp == 1 );
  mesh = parmesh->listgrp[0].mesh;

  /* A non-// tria may be marked as // in Medit serial I/O (if its 3 edges are
   * //): as we can infer // triangles from communicators, reset useless (and
   * maybe erroneous) tags */
  for ( k=1; k<=mesh->nt; ++k ) {
    if ( (mesh->tria[k].tag[0] & MG_PARBDY) &&
         (mesh->tria[k].tag[1] & MG_PARBDY) &&
         (mesh->tria[k].tag[2] & MG_PARBDY) ) {
      mesh->tria[k].tag[0] &= ~MG_PARBDY;
      mesh->tria[k].tag[1] &= ~MG_PARBDY;
      mesh->tria[k].tag[2] &= ~MG_PARBDY;
    }
  }

  /** Open mesh file */
  ier = MMG3D_openMesh(mesh->info.imprim,filename,&inm,&bin,"rb","rb");

  /** Read communicators */
  pos = 0;
  ncomm = 0;
  iswp = 0;
  API_mode = PMMG_UNSET;

  rewind(inm);
  if (!bin) {
    strcpy(chaine,"D");
    while(fscanf(inm,"%127s",&chaine[0])!=EOF && strncmp(chaine,"End",strlen("End")) ) {
      if ( chaine[0] == '#' ) {
        fgets(strskip,MMG5_FILESTR_LGTH,inm);
        continue;
      }

      if(!strncmp(chaine,"ParallelTriangleCommunicators",strlen("ParallelTriangleCommunicators"))) {
        MMG_FSCANF(inm,"%d",&ncomm);
        pos = ftell(inm);
        API_mode = PMMG_APIDISTRIB_faces;
        break;
      } else if(!strncmp(chaine,"ParallelVertexCommunicators",strlen("ParallelVertexCommunicators"))) {
        MMG_FSCANF(inm,"%d",&ncomm);
        pos = ftell(inm);
        API_mode = PMMG_APIDISTRIB_nodes;
        break;
      }
    }
  } else { //binary file
    MMG_FREAD(&meshver,MMG5_SW,1,inm);
    iswp=0;
    if(meshver==16777216)
      iswp=1;
    else if(meshver!=1) {
      fprintf(stderr,"BAD FILE ENCODING\n");
    }

    int endcount = 0;
    while(fread(&binch,MMG5_SW,1,inm)!=0 && endcount != 2 ) {
      if(iswp) binch=MMG5_swapbin(binch);
      if(binch==54) break;
      if(!ncomm && binch==70) { // ParallelTriangleCommunicators
        MMG_FREAD(&bpos,MMG5_SW,1,inm); //NulPos
        if(iswp) bpos=MMG5_swapbin(bpos);
        MMG_FREAD(&ncomm,MMG5_SW,1,inm);
        if(iswp) ncomm=MMG5_swapbin(ncomm);
        pos = ftell(inm);
        API_mode = PMMG_APIDISTRIB_faces;
        break; // if parallel triangles are found, ignore parallel nodes
      } else if(!ncomm && binch==71) { // ParallelVertexCommunicators
        MMG_FREAD(&bpos,MMG5_SW,1,inm); //NulPos
        if(iswp) bpos=MMG5_swapbin(bpos);
        MMG_FREAD(&ncomm,MMG5_SW,1,inm);
        if(iswp) ncomm=MMG5_swapbin(ncomm);
        pos = ftell(inm);
        API_mode = PMMG_APIDISTRIB_nodes;
        rewind(inm);
        fseek(inm,bpos,SEEK_SET);
        continue;
      } else if ( binch==54 ) {
        /* The end keyword will be present twice */
        ++endcount;
      } else {
        MMG_FREAD(&bpos,MMG5_SW,1,inm); //NulPos
        if(iswp) bpos=MMG5_swapbin(bpos);
        rewind(inm);
        fseek(inm,bpos,SEEK_SET);
      }
    }
  }

  /* Set API mode */
  if( API_mode == PMMG_UNSET ) {
    fprintf(stderr,"### Error: No parallel communicators provided on rank %d!\n",parmesh->myrank);
    return 0;
  } else if( !PMMG_Set_iparameter( parmesh, PMMG_IPARAM_APImode, API_mode ) ) {
    return 0;
  }

  /* memory allocation */
  PMMG_CALLOC(parmesh,nitem_comm,ncomm,int,"nitem_comm",return 0);
  PMMG_CALLOC(parmesh,color,ncomm,int,"color",return 0);
  PMMG_CALLOC(parmesh,idx_loc,ncomm,int*,"idx_loc pointer",return 0);
  PMMG_CALLOC(parmesh,idx_glo,ncomm,int*,"idx_glo pointer",return 0);

  /* Load the communicator */
  if( !PMMG_loadCommunicator( parmesh,inm,bin,iswp,pos,ncomm,nitem_comm,color,
                              idx_loc,idx_glo ) ) return 0;

  /* Set triangles or nodes interfaces depending on API mode */
  switch( API_mode ) {

    case PMMG_APIDISTRIB_faces :

      /* Set the number of interfaces */
      ier = PMMG_Set_numberOfFaceCommunicators(parmesh, ncomm);

      /* Loop on each interface (proc pair) seen by the current rank) */
      for( icomm = 0; icomm < ncomm; icomm++ ) {

        /* Set nb. of entities on interface and rank of the outward proc */
        ier = PMMG_Set_ithFaceCommunicatorSize(parmesh, icomm,
                                               color[icomm],
                                               nitem_comm[icomm]);

        /* Set local and global index for each entity on the interface */
        ier = PMMG_Set_ithFaceCommunicator_faces(parmesh, icomm,
                                                 idx_loc[icomm],
                                                 idx_glo[icomm], 1 );
      }
      break;

    case PMMG_APIDISTRIB_nodes :

      /* Set the number of interfaces */
      ier = PMMG_Set_numberOfNodeCommunicators(parmesh, ncomm);

      /* Loop on each interface (proc pair) seen by the current rank) */
      for( icomm = 0; icomm < ncomm; icomm++ ) {

        /* Set nb. of entities on interface and rank of the outward proc */
        ier = PMMG_Set_ithNodeCommunicatorSize(parmesh, icomm,
                                               color[icomm],
                                               nitem_comm[icomm]);

        /* Set local and global index for each entity on the interface */
        ier = PMMG_Set_ithNodeCommunicator_nodes(parmesh, icomm,
                                                 idx_loc[icomm],
                                                 idx_glo[icomm], 1 );
      }
      break;
  }

  /* Release memory and return */
  PMMG_DEL_MEM(parmesh,nitem_comm,int,"nitem_comm");
  PMMG_DEL_MEM(parmesh,color,int,"color");
  for( icomm = 0; icomm < ncomm; icomm++ ) {
    PMMG_DEL_MEM(parmesh,idx_loc[icomm],int,"idx_loc");
    PMMG_DEL_MEM(parmesh,idx_glo[icomm],int,"idx_glo");
  }
  PMMG_DEL_MEM(parmesh,idx_loc,int*,"idx_loc pointer");
  PMMG_DEL_MEM(parmesh,idx_glo,int*,"idx_glo pointer");

  return 1;
}

/**
 * \param parmesh pointer toward the parmesh structure
 * \param endame string to allocate to store the final filename
 * \param initname initial file name in which we want to insert rank index
 * \param ASCIIext extension to search for ASCII format
 * \param binext extension to search for binary format
 *
 * Allocate the endname string and copy the initname string with the mpir rank
 * index before the file extension.
 *
 */
static inline
void PMMG_insert_rankIndex(PMMG_pParMesh parmesh,char **endname,const char *initname,
                           char *ASCIIext, char *binext) {
  int    lenmax;
  int8_t fmt;
  char   *ptr;

  lenmax = PMMG_count_digits ( parmesh->nprocs );

  /* Check for pointer validity */
  if ( (!endname) || (!initname) ) {
    return;
  }

  MMG5_SAFE_CALLOC(*endname,strlen(initname)+lenmax+7,char,return);

  strcpy(*endname,initname);

  ptr = strstr(*endname,binext);

  fmt = 0; /* noext */
  if ( ptr ) {
    *ptr = '\0';
    fmt = 1; /* binary */
  }
  else {
    ptr = strstr(*endname,ASCIIext);
    if ( ptr ) {
      *ptr = '\0';
      fmt = 2; /* ASCII */
    }
  }
  sprintf(*endname, "%s.%d", *endname, parmesh->myrank );
  if ( fmt==1 ) {
    strcat ( *endname, binext );
  }
  else if ( fmt==2 ) {
    strcat ( *endname, ASCIIext );
  }

  return;
}

/**
 * \param parmesh pointer toward the parmesh structure.
 * \param filename name of the file to load the mesh from.
 *
 * \return 0 if fail, 1 otherwise
 *
 * Load a distributed mesh with parallel communicators in Medit format (only one
 * group per process is allowed). The rank index is inserted in the input file
 * name.
 *
 */
int PMMG_loadMesh_distributed(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh  mesh;
  int         ier;
  char*       data = NULL;

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }

  mesh = parmesh->listgrp[0].mesh;

  /* Add rank index to mesh name */
  if ( filename ) {
    PMMG_insert_rankIndex(parmesh,&data,filename,".mesh", ".meshb");
  }
  else if ( parmesh->meshin ) {
    PMMG_insert_rankIndex(parmesh,&data,parmesh->meshin,".mesh", ".meshb");
  }
  else if ( mesh->namein ) {
    PMMG_insert_rankIndex(parmesh,&data,mesh->namein,".mesh", ".meshb");
  }

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  ier = MMG3D_loadMesh(mesh,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  if ( ier < 1 ) {
    MMG5_SAFE_FREE(data);
    return ier;
  }

  /* Load parallel communicators */
  ier = PMMG_loadCommunicators( parmesh,data );

  MMG5_SAFE_FREE(data);

  if ( 1 != ier ) return 0;

  return 1;
}

int PMMG_loadMesh_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  int        ier;
  const char *data;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename ) {
    data = filename;
  }
  else if ( parmesh->meshin ) {
    data = parmesh->meshin;
  }
  else if ( mesh->namein ) {
    data = mesh->namein;
  }
  else {
    data = NULL;
  }
  ier = MMG3D_loadMesh(mesh,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_loadMet_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  met;
  int        ier;
  const char *data;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;
  met  = parmesh->listgrp[0].met;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename ) {
    data = filename;
  }
  else if ( parmesh->metin ) {
    data = parmesh->metin;
  }
  else if ( met->namein ) {
    data = met->namein;
  }
  else {
    data = NULL;
  }
  ier = MMG3D_loadSol(mesh,met,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_loadMet_distributed(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  met;
  int        ier;
  char       *data = NULL;

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }

  mesh = parmesh->listgrp[0].mesh;
  met  = parmesh->listgrp[0].met;

  /* Add rank index to mesh name */
  if ( filename ) {
    PMMG_insert_rankIndex(parmesh,&data,filename,".sol", ".sol");
  }
  else if ( parmesh->metin ) {
    PMMG_insert_rankIndex(parmesh,&data,parmesh->metin,".sol", ".sol");
  }
  else if ( met->namein ) {
    PMMG_insert_rankIndex(parmesh,&data,met->namein,".sol", ".sol");
  }
  else if ( parmesh->meshin ) {
    PMMG_insert_rankIndex(parmesh,&data,parmesh->meshin,".mesh", ".meshb");
  }
  else if ( mesh->namein ) {
    PMMG_insert_rankIndex(parmesh,&data,mesh->namein,".mesh", ".meshb");
  }

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  ier = MMG3D_loadSol(mesh,met,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  MMG5_SAFE_FREE(data);

  return ier;
}

int PMMG_loadLs_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  ls;
  int        ier;
  const char *data;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;
  ls   = parmesh->listgrp[0].ls;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename ) {
    data = filename;
  }
  else if ( parmesh->lsin ) {
    data = parmesh->lsin;
  }
  else if ( ls->namein ) {
    data = ls->namein;
  }
  else {
    data = NULL;
  }
  ier = MMG3D_loadSol(mesh,ls,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_loadDisp_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  disp;
  int        ier;
  const char *data;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;
  disp = parmesh->listgrp[0].disp;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename ) {
    data = filename;
  }
  else if ( parmesh->dispin ) {
    data = parmesh->dispin;
  }
  else if ( disp->namein ) {
    data = disp->namein;
  }
  else {
    data = NULL;
  }
  ier = MMG3D_loadSol(mesh,disp,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_loadSol_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  sol;
  int        ier;
  const char *namein;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;

  /* For each mode: pointer over the solution structure to load */
  if ( mesh->info.lag >= 0 ) {
    sol = parmesh->listgrp[0].disp;
  }
  else if ( mesh->info.iso ) {
    sol = parmesh->listgrp[0].ls;
  }
  else {
    sol = parmesh->listgrp[0].met;
  }

  if ( !filename ) {
    namein = sol->namein;
  }
  else {
    namein = filename;
  }
  assert ( namein );

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  ier = MMG3D_loadSol(mesh,sol,namein);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_loadAllSols_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  *sol;
  int        ier;
  const char *data;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;
  sol  = &parmesh->listgrp[0].field;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename ) {
    data = filename;
  }
  else if ( parmesh->fieldin ) {
    data = parmesh->fieldin;
  }
  else {
    data = NULL;
  }
  ier = MMG3D_loadAllSols(mesh,sol,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;

}

/**
 * \param parmesh pointer toward the parmesh structure.
 * \param filename name of the file to load the mesh from.
 *
 * \return 0 if fail, 1 otherwise
 *
 * Save a distributed mesh with parallel communicators in Medit format (only one
 * group per process is allowed).
 *
 */
int PMMG_saveMesh_distributed(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh  mesh;
  int         ier;
  char       *data = NULL;

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }

  mesh = parmesh->listgrp[0].mesh;

  /* Add rank index to mesh name */
  if ( filename ) {
    PMMG_insert_rankIndex(parmesh,&data,filename,".mesh", ".meshb");
  }
  else if ( parmesh->meshout ) {
    PMMG_insert_rankIndex(parmesh,&data,parmesh->meshout,".mesh", ".meshb");
  }
  else if ( mesh->nameout ) {
    PMMG_insert_rankIndex(parmesh,&data,mesh->nameout,".mesh", ".meshb");
  }

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  ier = MMG3D_saveMesh(mesh,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  if ( ier < 1 ) {
    MMG5_SAFE_FREE(data);
    return ier;
  }

  /* Load parallel communicators */
  ier = PMMG_printCommunicator ( parmesh,data );

  MMG5_SAFE_FREE ( data );

  if ( 1 != ier ) return 0;

  return 1;
}


int PMMG_saveMesh_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  int        ier;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename && *filename ) {
    ier = MMG3D_saveMesh(mesh,filename);
  }
  else {
    ier = MMG3D_saveMesh(mesh,parmesh->meshout);
  }

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_saveMet_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  met;
  int        ier;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;
  met  = parmesh->listgrp[0].met;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename && *filename ) {
    ier =  MMG3D_saveSol(mesh,met,filename);
  }
  else {
    ier =  MMG3D_saveSol(mesh,met,parmesh->metout);
  }

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_saveMet_distributed(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  met;
  int        ier;
  char       *data = NULL;

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }

  mesh = parmesh->listgrp[0].mesh;
  met  = parmesh->listgrp[0].met;

  /* Add rank index to mesh name */
  if ( filename ) {
    PMMG_insert_rankIndex(parmesh,&data,filename,".sol", ".sol");
  }
  else if ( parmesh->metout ) {
    PMMG_insert_rankIndex(parmesh,&data,parmesh->metout,".sol", ".sol");
  }
  else if ( met->nameout ) {
    PMMG_insert_rankIndex(parmesh,&data,met->nameout,".sol", ".sol");
  }
  else if ( parmesh->meshout ) {
    PMMG_insert_rankIndex(parmesh,&data,parmesh->meshout,".mesh", ".meshb");
  }
  else if ( mesh->nameout ) {
    PMMG_insert_rankIndex(parmesh,&data,mesh->nameout,".mesh", ".meshb");
  }

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  ier =  MMG3D_saveSol(mesh,met,data);

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  MMG5_SAFE_FREE ( data );

  return ier;
}

int PMMG_saveAllSols_centralized(PMMG_pParMesh parmesh,const char *filename) {
  MMG5_pMesh mesh;
  MMG5_pSol  sol;
  int        ier;

  if ( parmesh->myrank!=parmesh->info.root ) {
    return 1;
  }

  if ( parmesh->ngrp != 1 ) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  mesh = parmesh->listgrp[0].mesh;
  sol  = parmesh->listgrp[0].field;

  /* Set mmg verbosity to the max between the Parmmg verbosity and the mmg verbosity */
  assert ( mesh->info.imprim == parmesh->info.mmg_imprim );
  mesh->info.imprim = MG_MAX ( parmesh->info.imprim, mesh->info.imprim );

  if ( filename && *filename ) {
    ier = MMG3D_saveAllSols(mesh,&sol,filename);
  }
  else {
    ier = MMG3D_saveAllSols(mesh,&sol,parmesh->fieldout);
  }

  /* Restore the mmg verbosity to its initial value */
  mesh->info.imprim = parmesh->info.mmg_imprim;

  return ier;
}

int PMMG_saveParmesh_hdf5(PMMG_pParMesh parmesh, const char *filename, const char *xdmfname) {
  /* MMG variables */
  int ier = MMG5_SUCCESS;
  int nsols;
  PMMG_pGrp grp;
  MMG5_pSol met, sols;
  MMG5_pMesh mesh;
  MMG5_pPoint ppt;
  MMG5_pEdge pa;
  MMG5_pTria pt;
  MMG5_pQuad pq;
  MMG5_pTetra pe;
  MMG5_pPrism pp;

  /* Local mesh size */
  int ne, np, nt, na, nquad, nprism;       /* Tetra, points, triangles, edges, quads, prisms */
  int nc, nreq, npar;                      /* Corners, required and parallel vertices */
  int nr, nedreq, nedpar;                  /* Ridges, required and parallel edges */
  int ntreq, ntpar;                        /* Required and parallel triangles */
  int nqreq, nqpar;                        /* Required and parallel quads */
  int nereq, nepar;                        /* Required and parallel tetra */

  /* Global mesh size */
  int neg, npg, ntg, nag, nquadg, nprismg; /* Tetra, points, triangles, edges, quads, prisms */
  int ncg, nreqg, nparg;                   /* Corners, required and parallel vertices */
  int nrg, nedreqg, nedparg;               /* Ridges, required and parallel edges */
  int ntreqg, ntparg;                      /* Required and parallel triangles */
  int nqreqg, nqparg;                      /* Required and parallel quads */
  int nereqg, neparg;                      /* Required and parallel tetra */

  /* Buffer arrays */
  double *ppoint;
  int *pedge, *ptria, *pquad, *ptet, *pprism;
  int *pcorner, *preq, *ppar;
  int *pridges, *pedreq, *pedpar;
  int *ptreq, *ptpar;
  int *pqreq, *pqpar;
  int *ptetreq, *ptetpar;
  int *ppoint_ref, *pedge_ref, *ptria_ref, *pquad_ref, *ptet_ref, *pprism_ref;

  /* Store the number of entities on each proc */
  int *nentities;

  /* Counters for the corners, the ridges, the required entities and the parallel entities */
  int crcount, reqcount, parcount;

  /* Offsets for parallel writing */
  hsize_t point_offset[3] = {0, 0, 0};
  hsize_t edge_offset[2]  = {0, 0};
  hsize_t tria_offset[3]  = {0, 0, 0};
  hsize_t quad_offset[4]  = {0, 0, 0, 0};
  hsize_t tetra_offset[4] = {0, 0, 0, 0};
  hsize_t prism_offset[6] = {0, 0, 0, 0, 0, 0};
  hsize_t required_offset[5] = {0, 0, 0, 0, 0};     /* Used for the required entities */
  hsize_t parallel_offset[5] = {0, 0, 0, 0, 0};     /* Used for the parallel entities */
  hsize_t corner_offset = 0;                        /* Used for the corners */
  hsize_t ridge_offset = 0;                         /* Used for the ridges */

  /* HDF5 variables */
  hid_t file_id, grp_mesh_id, grp_sols_id;
  hid_t fapl_id, dxpl_id;

  hid_t dspace_mem_point_id, dspace_file_point_id;
  hid_t dspace_mem_edge_id, dspace_file_edge_id;
  hid_t dspace_mem_tria_id, dspace_file_tria_id;
  hid_t dspace_mem_quad_id, dspace_file_quad_id;
  hid_t dspace_mem_tetra_id, dspace_file_tetra_id;
  hid_t dspace_mem_prism_id, dspace_file_prism_id;

  hid_t dspace_mem_ref_point_id, dspace_file_ref_point_id;
  hid_t dspace_mem_ref_edge_id, dspace_file_ref_edge_id;
  hid_t dspace_mem_ref_tria_id, dspace_file_ref_tria_id;
  hid_t dspace_mem_ref_quad_id, dspace_file_ref_quad_id;
  hid_t dspace_mem_ref_tetra_id, dspace_file_ref_tetra_id;
  hid_t dspace_mem_ref_prism_id, dspace_file_ref_prism_id;

  hid_t dspace_mem_corner_id, dspace_file_corner_id;
  hid_t dspace_mem_req_id, dspace_file_req_id;
  hid_t dspace_mem_par_id, dspace_file_par_id;

  hid_t dspace_mem_ridge_id, dspace_file_ridge_id;
  hid_t dspace_mem_edreq_id, dspace_file_edreq_id;
  hid_t dspace_mem_edpar_id, dspace_file_edpar_id;

  hid_t dspace_mem_treq_id, dspace_file_treq_id;
  hid_t dspace_mem_tpar_id, dspace_file_tpar_id;

  hid_t dspace_mem_qreq_id, dspace_file_qreq_id;
  hid_t dspace_mem_qpar_id, dspace_file_qpar_id;

  hid_t dspace_mem_tetreq_id, dspace_file_tetreq_id;
  hid_t dspace_mem_tetpar_id, dspace_file_tetpar_id;

  hid_t dset_id;
  herr_t status;

  /* MPI variables */
  MPI_Info info = MPI_INFO_NULL;
  MPI_Comm comm = parmesh->comm;
  int rank, root, nprocs;

  /*------------------------- INIT -------------------------*/

  /* Set all buffers to NULL */
  ppoint = NULL; ppoint_ref = NULL;
  pedge  = NULL; pedge_ref  = NULL;
  ptria  = NULL; ptria_ref  = NULL;
  pquad  = NULL; pquad_ref  = NULL;
  ptet   = NULL; ptet_ref   = NULL;
  pprism = NULL; pprism_ref = NULL;
  pcorner = NULL; preq = NULL; ppar = NULL;
  pridges = NULL; pedreq = NULL; pedpar = NULL;
  ptreq  = NULL; ptpar = NULL;
  pqreq = NULL; pqpar = NULL;
  ptetreq = NULL; ptetpar = NULL;
  nentities = NULL;

  /* Set MPI variables */
  MPI_Comm_size(comm, &nprocs);
  MPI_Comm_rank(comm, &rank);
  root = parmesh->info.root;

  /* Set local mesh size to 0 */
  np = na = nt = nquad = ne = nprism = 0;
  nc = nreq = npar = 0;
  nr = nedreq = nedpar = 0;
  ntreq = ntpar = 0;
  nqreq = nqpar = 0;
  nereq = nepar = 0;
  /* Set global mesh size to 0 */
  npg = nag = ntg = nquadg = neg = nprismg = 0;
  ncg = nreqg = nparg = 0;
  nrg = nedreqg = nedparg = 0;
  ntreqg = ntparg = 0;
  nqreqg = nqparg = 0;
  nereqg = neparg = 0;

  /* Check arguments */
  if (parmesh->ngrp != 1) {
    fprintf(stderr,"  ## Error: %s: you must have exactly 1 group in you parmesh.",
            __func__);
    return 0;
  }
  if (!filename || !*filename) {
    fprintf(stderr,"  ## Error: %s: no HDF5 file name provided.",
            __func__);
    return 0;
  }

  /* Set ParMmg variables */
  grp = &parmesh->listgrp[0];
  mesh = grp->mesh;
  met = grp->met;
  sols = grp->field;
  nsols = mesh->nsols;
  ppt = NULL;
  pa = NULL;
  pt = NULL;
  pq = NULL;
  pe = NULL;
  pp = NULL;

  /*------------------------- COUNT LOCAL MESH ENTITIES -------------------------*/

  if ( !mesh->point ) {
    fprintf(stderr, "\n  ## Error: %s: points array not allocated.\n",
            __func__);
    return 0;
  }

  /* Vertices */
  for (int k = 1 ; k <= mesh->np ; k++) {
    ppt = &mesh->point[k];
    if (MG_VOK(ppt)) {
      ppt->tmp = ++np;
      ppt->flag = 0;
      if (ppt->tag & MG_CRN) nc++;
      if (ppt->tag & MG_REQ) nreq++;
      if (ppt->tag & MG_PARBDY) npar++;
    }
  }

  /* Edges */
  if (mesh->na) {
    for (int k = 1 ; k <= mesh->na ; k++) {
      pa = &mesh->edge[k];
      na++;
      if (pa->tag & MG_GEO) nr++;
      if (pa->tag & MG_REQ) nedreq++;
      if (pa->tag & MG_PARBDY) nedpar++;
    }
  }

  /* Triangles */
  if (mesh->nt) {
    for (int k = 1 ; k <= mesh->nt ; k++) {
      pt = &mesh->tria[k];
      nt++;
      if (pt->tag[0] & MG_REQ && pt->tag[1] & MG_REQ && pt->tag[2] & MG_REQ) ntreq++;
      if (pt->tag[0] & MG_PARBDY && pt->tag[1] & MG_PARBDY && pt->tag[2] & MG_PARBDY) ntpar++;
    }
  }

  /* Quadrilaterals */
  if (mesh->nquad) {
    for (int k = 1 ; k <= mesh->nquad ; k++) {
      pq = &mesh->quadra[k];
      nquad++;
      if (pq->tag[0] & MG_REQ && pq->tag[1] & MG_REQ &&
          pq->tag[2] & MG_REQ && pq->tag[3] & MG_REQ) {
        nqreq++;
      }
      if (pq->tag[0] & MG_PARBDY && pq->tag[1] & MG_PARBDY &&
          pq->tag[2] & MG_PARBDY && pq->tag[3] & MG_PARBDY) {
        nqpar++;
      }
    }
  }

  /* Tetrahedra */
  if (mesh->ne) {
    for (int k = 1 ; k <= mesh->ne ; k++) {
      pe = &mesh->tetra[k];
      if (!MG_EOK(pe)) {
        continue;
      }
      ne++;
      if (pe->tag & MG_REQ) nereq++;
      if (pe->tag & MG_PARBDY) nepar++;
    }
  } else {
    fprintf(stderr, "\n  ## Warning: %s: tetra array not allocated.\n",
            __func__);
  }

  /* Prisms */
  if (mesh->nprism) {
    for (int k = 1 ; k <= mesh->nprism ; k++) {
      pp = &mesh->prism[k];
      if (!MG_EOK(pp)){
        continue;
      }
      nprism++;
    }
  }

  /*------------------------- COUNT GLOBAL MESH ENTITIES -------------------------*/

  nentities = (int*) calloc(18 * nprocs, sizeof(int));

  nentities[18 * rank]      = np;
  nentities[18 * rank + 1]  = na;
  nentities[18 * rank + 2]  = nt;
  nentities[18 * rank + 3]  = nquad;
  nentities[18 * rank + 4]  = ne;
  nentities[18 * rank + 5]  = nprism;
  nentities[18 * rank + 6]  = nc;
  nentities[18 * rank + 7]  = nreq;
  nentities[18 * rank + 8]  = npar;
  nentities[18 * rank + 9]  = nr;
  nentities[18 * rank + 10] = nedreq;
  nentities[18 * rank + 11] = nedpar;
  nentities[18 * rank + 12] = ntreq;
  nentities[18 * rank + 13] = ntpar;
  nentities[18 * rank + 14] = nqreq;
  nentities[18 * rank + 15] = nqpar;
  nentities[18 * rank + 16] = nereq;
  nentities[18 * rank + 17] = nepar;

  MPI_Allgather(&nentities[18 * rank], 18, MPI_INT, nentities, 18, MPI_INT, comm);

  for (int k = 0 ; k < nprocs ; k++) {
    npg     += nentities[18 * k];
    nag     += nentities[18 * k + 1];
    ntg     += nentities[18 * k + 2];
    nquadg  += nentities[18 * k + 3];
    neg     += nentities[18 * k + 4];
    nprismg += nentities[18 * k + 5];
    ncg     += nentities[18 * k + 6];
    nreqg   += nentities[18 * k + 7];
    nparg   += nentities[18 * k + 8];
    nrg     += nentities[18 * k + 9];
    nedreqg += nentities[18 * k + 10];
    nedparg += nentities[18 * k + 11];
    ntreqg  += nentities[18 * k + 12];
    ntparg  += nentities[18 * k + 13];
    nqreqg  += nentities[18 * k + 14];
    nqparg  += nentities[18 * k + 15];
    nereqg  += nentities[18 * k + 16];
    neparg  += nentities[18 * k + 17];
  }

  /*------------------------- COMPUTE OFFSET ARRAYS -------------------------*/
  for (int i = 0 ; i < rank ; i++) {
    point_offset[0]    += nentities[18 * i];
    edge_offset[0]     += nentities[18 * i + 1];
    tria_offset[0]     += nentities[18 * i + 2];
    quad_offset[0]     += nentities[18 * i + 3];
    tetra_offset[0]    += nentities[18 * i + 4];
    prism_offset[0]    += nentities[18 * i + 5];
    corner_offset      += nentities[18 * i + 6];
    required_offset[0] += nentities[18 * i + 7];
    parallel_offset[0] += nentities[18 * i + 8];
    ridge_offset       += nentities[18 * i + 9];
    required_offset[1] += nentities[18 * i + 10];
    parallel_offset[1] += nentities[18 * i + 11];
    required_offset[2] += nentities[18 * i + 12];
    parallel_offset[2] += nentities[18 * i + 13];
    required_offset[3] += nentities[18 * i + 14];
    parallel_offset[3] += nentities[18 * i + 15];
    required_offset[4] += nentities[18 * i + 16];
    parallel_offset[4] += nentities[18 * i + 17];
  }

  /*------------------------- HDF5 IOs START HERE -------------------------*/

  /* Shut HDF5 error stack */
  H5Eset_auto(H5E_DEFAULT, NULL, NULL);

  /* Create the parallel file acces and the parallel dataset transfer property lists */
  fapl_id = H5Pcreate(H5P_FILE_ACCESS);
  status = H5Pset_fapl_mpio(fapl_id, comm, info);
  dxpl_id = H5Pcreate(H5P_DATASET_XFER);
  status = H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_COLLECTIVE);

  /*------------------------- CREATE ALL HDF5 DATASPACES -------------------------*/

  hsize_t hnp[2]      = {np,3};
  hsize_t hna[2]      = {na,2};
  hsize_t hnt[2]      = {nt,3};
  hsize_t hnquad[2]   = {nquad,4};
  hsize_t hne[2]      = {ne,4};
  hsize_t hnprism[2]  = {nprism,2};
  hsize_t hnc         = nc;
  hsize_t hnreq       = nreq;
  hsize_t hnpar       = npar;
  hsize_t hnr         = nr;
  hsize_t hnedreq     = nedreq;
  hsize_t hnedpar     = nedpar;
  hsize_t hntreq      = ntreq;
  hsize_t hntpar      = ntpar;
  hsize_t hnqreq      = nqreq;
  hsize_t hnqpar      = nqpar;
  hsize_t hnereq      = nereq;
  hsize_t hnepar      = nepar;

  dspace_mem_point_id = H5Screate_simple(2, hnp    , NULL);
  dspace_mem_edge_id  = H5Screate_simple(2, hna    , NULL);
  dspace_mem_tria_id  = H5Screate_simple(2, hnt    , NULL);
  dspace_mem_quad_id  = H5Screate_simple(2, hnquad , NULL);
  dspace_mem_tetra_id = H5Screate_simple(2, hne    , NULL);
  dspace_mem_prism_id = H5Screate_simple(2, hnprism, NULL);

  dspace_mem_ref_point_id = H5Screate_simple(1, hnp    , NULL);
  dspace_mem_ref_edge_id  = H5Screate_simple(1, hna    , NULL);
  dspace_mem_ref_tria_id  = H5Screate_simple(1, hnt    , NULL);
  dspace_mem_ref_quad_id  = H5Screate_simple(1, hnquad , NULL);
  dspace_mem_ref_tetra_id = H5Screate_simple(1, hne    , NULL);
  dspace_mem_ref_prism_id = H5Screate_simple(1, hnprism, NULL);

  dspace_mem_corner_id = H5Screate_simple(1, &hnc, NULL);
  dspace_mem_req_id = H5Screate_simple(1, &hnreq, NULL);
  dspace_mem_par_id = H5Screate_simple(1, &hnpar, NULL);

  dspace_mem_ridge_id = H5Screate_simple(1, &hnr, NULL);
  dspace_mem_edreq_id = H5Screate_simple(1, &hnedreq, NULL);
  dspace_mem_edpar_id = H5Screate_simple(1, &hnedpar, NULL);

  dspace_mem_treq_id = H5Screate_simple(1, &hntreq, NULL);
  dspace_mem_tpar_id = H5Screate_simple(1, &hntpar, NULL);

  dspace_mem_qreq_id = H5Screate_simple(1, &hnqreq, NULL);
  dspace_mem_qpar_id = H5Screate_simple(1, &hnqpar, NULL);

  dspace_mem_tetreq_id = H5Screate_simple(1, &hnereq, NULL);
  dspace_mem_tetpar_id = H5Screate_simple(1, &hnepar, NULL);

  hsize_t hnpg[2]     = {npg,3};
  hsize_t hnag[2]     = {nag,2};
  hsize_t hntg[2]     = {ntg,3};
  hsize_t hnquadg[2]  = {nquadg,4};
  hsize_t hneg[2]     = {neg,4};
  hsize_t hnprismg[2] = {nprismg,2};
  hsize_t hncg         = ncg;
  hsize_t hnreqg       = nreqg;
  hsize_t hnparg       = nparg;
  hsize_t hnrg         = nrg;
  hsize_t hnedreqg     = nedreqg;
  hsize_t hnedparg     = nedparg;
  hsize_t hntreqg      = ntreqg;
  hsize_t hntparg      = ntparg;
  hsize_t hnqreqg      = nqreqg;
  hsize_t hnqparg      = nqparg;
  hsize_t hnereqg      = nereqg;
  hsize_t hneparg      = neparg;

  dspace_file_point_id = H5Screate_simple(2, hnpg    , NULL);
  dspace_file_edge_id  = H5Screate_simple(2, hnag    , NULL);
  dspace_file_tria_id  = H5Screate_simple(2, hntg    , NULL);
  dspace_file_quad_id  = H5Screate_simple(2, hnquadg , NULL);
  dspace_file_tetra_id = H5Screate_simple(2, hneg    , NULL);
  dspace_file_prism_id = H5Screate_simple(2, hnprismg, NULL);

  dspace_file_ref_point_id = H5Screate_simple(1, hnpg    , NULL);
  dspace_file_ref_edge_id  = H5Screate_simple(1, hnag    , NULL);
  dspace_file_ref_tria_id  = H5Screate_simple(1, hntg    , NULL);
  dspace_file_ref_quad_id  = H5Screate_simple(1, hnquadg , NULL);
  dspace_file_ref_tetra_id = H5Screate_simple(1, hneg    , NULL);
  dspace_file_ref_prism_id = H5Screate_simple(1, hnprismg, NULL);

  dspace_file_corner_id = H5Screate_simple(1, &hncg, NULL);
  dspace_file_req_id = H5Screate_simple(1, &hnreqg, NULL);
  dspace_file_par_id = H5Screate_simple(1, &hnparg, NULL);

  dspace_file_ridge_id = H5Screate_simple(1, &hnrg, NULL);
  dspace_file_edreq_id = H5Screate_simple(1, &hnedreqg, NULL);
  dspace_file_edpar_id = H5Screate_simple(1, &hnedparg, NULL);

  dspace_file_treq_id = H5Screate_simple(1, &hntreqg, NULL);
  dspace_file_tpar_id = H5Screate_simple(1, &hntparg, NULL);

  dspace_file_qreq_id = H5Screate_simple(1, &hnqreqg, NULL);
  dspace_file_qpar_id = H5Screate_simple(1, &hnqparg, NULL);

  dspace_file_tetreq_id = H5Screate_simple(1, &hnereqg, NULL);
  dspace_file_tetpar_id = H5Screate_simple(1, &hneparg, NULL);


  /*------------------------- OPEN FILE AND WRITE DATA -------------------------*/

  /* Create the file and the groups */
  file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);
  grp_mesh_id = H5Gcreate(file_id, "mesh_grp", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  grp_sols_id = H5Gcreate(file_id, "sols_grp", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

  /* Vertices */
  ppoint = (double*) calloc(np, 3 * sizeof(double));
  ppoint_ref = (int*) calloc(np, sizeof(int));
  pcorner = (int*) calloc(nc, sizeof(int));
  preq = (int*) calloc(nreq, sizeof(int));
  ppar = (int*) calloc(npar, sizeof(int));

  crcount = reqcount = parcount = 0;

  for (int i = 0 ; i < mesh->np ; i++) {
    ppt = &mesh->point[i + 1];
    if (MG_VOK(ppt)){
      for (int j = 0 ; j < 3 ; j++) {
        ppoint[3 * (ppt->tmp - 1) + j] = ppt->c[j];
      }
      if (ppt->tag & MG_CRN)    pcorner[crcount++] = ppt->tmp + point_offset[0] - 1;
      if (ppt->tag & MG_REQ)    preq[reqcount++]   = ppt->tmp + point_offset[0] - 1;
      if (ppt->tag & MG_PARBDY) ppar[parcount++]   = ppt->tmp + point_offset[0] - 1;
      ppoint_ref[ppt->tmp - 1] = abs(ppt->ref);
    }
  }

  status = H5Sselect_hyperslab(dspace_file_point_id, H5S_SELECT_SET, point_offset, NULL, hnp, NULL);
  status = H5Sselect_hyperslab(dspace_file_ref_point_id, H5S_SELECT_SET, point_offset, NULL, hnp, NULL);
  status = H5Sselect_hyperslab(dspace_file_corner_id, H5S_SELECT_SET, &corner_offset, NULL, &hnc, NULL);
  status = H5Sselect_hyperslab(dspace_file_req_id, H5S_SELECT_SET, &required_offset[0], NULL, &hnreq, NULL);
  status = H5Sselect_hyperslab(dspace_file_par_id, H5S_SELECT_SET, &parallel_offset[0], NULL, &hnpar, NULL);

  dset_id = H5Dcreate(grp_mesh_id, "Vertices", H5T_NATIVE_DOUBLE, dspace_file_point_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_DOUBLE, dspace_mem_point_id, dspace_file_point_id, dxpl_id, ppoint);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "VerticesRef", H5T_NATIVE_INT, dspace_file_ref_point_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_ref_point_id, dspace_file_ref_point_id, dxpl_id, ppoint_ref);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "Corners", H5T_NATIVE_INT, dspace_file_corner_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_corner_id, dspace_file_corner_id, dxpl_id, pcorner);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "RequiredVertices", H5T_NATIVE_INT, dspace_file_req_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_req_id, dspace_file_req_id, dxpl_id, preq);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "ParallelVertices", H5T_NATIVE_INT, dspace_file_par_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_par_id, dspace_file_par_id, dxpl_id, ppar);
  H5Dclose(dset_id);

  free(ppoint); ppoint = NULL;
  free(ppoint_ref); ppoint_ref = NULL;
  free(pcorner) ; pcorner = NULL;
  free(preq) ; preq = NULL;
  free(ppar) ; ppar = NULL;

  /* Edges */
  pedge = (int*) calloc(na, 2 * sizeof(int));
  pedge_ref = (int*) calloc(na, sizeof(int));
  pridges = (int*) calloc(nr, sizeof(int));
  pedreq = (int*) calloc(nedreq, sizeof(int));
  pedpar = (int*) calloc(nedpar, sizeof(int));

  crcount = reqcount = parcount = 0;

  if (na) {
    na = 0;
    for (int i = 0 ; i < mesh->na ; i++) {
      pa = &mesh->edge[i + 1];
      pedge[2 * i]     = mesh->point[pa->a].tmp + point_offset[0] - 1;
      pedge[2 * i + 1] = mesh->point[pa->b].tmp + point_offset[0] - 1;
      pedge_ref[i] = pa->ref;
      if (pa->tag & MG_GEO)    pridges[crcount++] = na + edge_offset[0];
      if (pa->tag & MG_REQ)    pedreq[reqcount++] = na + edge_offset[0];
      if (pa->tag & MG_PARBDY) pedpar[parcount++] = na + edge_offset[0];
      na++;
    }
  }

  status = H5Sselect_hyperslab(dspace_file_edge_id, H5S_SELECT_SET, edge_offset, NULL, hna, NULL);
  status = H5Sselect_hyperslab(dspace_file_ref_edge_id, H5S_SELECT_SET, edge_offset, NULL, hna, NULL);
  status = H5Sselect_hyperslab(dspace_file_ridge_id, H5S_SELECT_SET, &ridge_offset, NULL, &hnr, NULL);
  status = H5Sselect_hyperslab(dspace_file_edreq_id, H5S_SELECT_SET, &required_offset[1], NULL, &hnedreq, NULL);
  status = H5Sselect_hyperslab(dspace_file_edpar_id, H5S_SELECT_SET, &parallel_offset[1], NULL, &hnedpar, NULL);

  dset_id = H5Dcreate(grp_mesh_id, "Edges", H5T_NATIVE_INT, dspace_file_edge_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_edge_id, dspace_file_edge_id, dxpl_id, pedge);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "EdgesRef", H5T_NATIVE_INT, dspace_file_ref_edge_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_ref_edge_id, dspace_file_ref_edge_id, dxpl_id, pedge_ref);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "Ridges", H5T_NATIVE_INT, dspace_file_ridge_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_ridge_id, dspace_file_ridge_id, dxpl_id, pridges);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "RequiredEdges", H5T_NATIVE_INT, dspace_file_edreq_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_edreq_id, dspace_file_edreq_id, dxpl_id, pedreq);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "ParallelEdges", H5T_NATIVE_INT, dspace_file_edpar_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_edpar_id, dspace_file_edpar_id, dxpl_id, pedpar);
  H5Dclose(dset_id);

  free(pedge); pedge = NULL;
  free(pedge); pedge_ref = NULL;
  free(pridges); pridges = NULL;
  free(pedreq) ; pedreq = NULL;
  free(pedpar) ; pedpar = NULL;

  /* Triangles */
  ptria = (int*) calloc(nt, 3 * sizeof(int));
  ptria_ref = (int*) calloc(nt, sizeof(int));
  ptreq = (int*) calloc(ntreq, sizeof(int));
  ptpar = (int*) calloc(ntpar, sizeof(int));

  reqcount = parcount = 0;

  if (nt) {
    nt = 0;
    for (int i = 0 ; i < mesh->nt ; i++) {
      pt = &mesh->tria[i + 1];
      for (int j = 0 ; j < 3 ; j++) {
        ptria[3 * i + j] = mesh->point[pt->v[j]].tmp + point_offset[0] - 1;
      }
      ptria_ref[i] = pt->ref;
      if (pt->tag[0] & MG_REQ && pt->tag[1] & MG_REQ && pt->tag[2] & MG_REQ) {
        ptreq[reqcount++] = nt + tria_offset[0];
      }
      if (pt->tag[0] & MG_PARBDY && pt->tag[1] & MG_PARBDY && pt->tag[2] & MG_PARBDY) {
        ptpar[parcount++] = nt + tria_offset[0];
      }
      nt++;
    }
  }

  status = H5Sselect_hyperslab(dspace_file_tria_id, H5S_SELECT_SET, tria_offset, NULL, hnt, NULL);
  status = H5Sselect_hyperslab(dspace_file_ref_tria_id, H5S_SELECT_SET, tria_offset, NULL, hnt, NULL);
  status = H5Sselect_hyperslab(dspace_file_treq_id, H5S_SELECT_SET, &required_offset[2], NULL, &hntreq, NULL);
  status = H5Sselect_hyperslab(dspace_file_tpar_id, H5S_SELECT_SET, &parallel_offset[2], NULL, &hntpar, NULL);

  dset_id = H5Dcreate(grp_mesh_id, "Triangles", H5T_NATIVE_INT, dspace_file_tria_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_tria_id, dspace_file_tria_id, dxpl_id, ptria);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "TrianglesRef", H5T_NATIVE_INT, dspace_file_ref_tria_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_ref_tria_id, dspace_file_ref_tria_id, dxpl_id, ptria_ref);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "RequiredTriangles", H5T_NATIVE_INT, dspace_file_treq_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_treq_id, dspace_file_treq_id, dxpl_id, ptreq);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "ParallelTriangles", H5T_NATIVE_INT, dspace_file_tpar_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_tpar_id, dspace_file_tpar_id, dxpl_id, ptpar);
  H5Dclose(dset_id);

  free(ptria); ptria = NULL;
  free(ptria); ptria_ref = NULL;
  free(ptreq); ptreq = NULL;
  free(ptpar); ptpar = NULL;

  /* Quadrilaterals */
  pquad = (int*) calloc(nquad, 4 * sizeof(int));
  pquad_ref = (int*) calloc(nquad, sizeof(int));
  pqreq = (int*) calloc(nqreq, sizeof(int));
  pqpar = (int*) calloc(nqpar, sizeof(int));

  reqcount = parcount = 0;

  if (nquad){
    nquad = 0;
    for (int i = 0 ; i < mesh->nquad ; i++) {
      pq = &mesh->quadra[i + 1];
      for (int j = 0 ; j < 4 ; j++) {
        pquad[4 * i + j] = mesh->point[pq->v[j]].tmp + point_offset[0] - 1;
      }
      pquad_ref[i] = pq->ref;
      if (pq->tag[0] & MG_REQ && pq->tag[1] & MG_REQ &&
          pq->tag[2] & MG_REQ && pq->tag[3] & MG_REQ) {
        pqreq[reqcount++] = nquad + quad_offset[0];
      }
      if (pq->tag[0] & MG_PARBDY && pq->tag[1] & MG_PARBDY &&
          pq->tag[2] & MG_PARBDY && pq->tag[3] & MG_PARBDY) {
        pqpar[parcount++] = nquad + quad_offset[0];
      }
      nquad++;
    }
  }

  status = H5Sselect_hyperslab(dspace_file_quad_id, H5S_SELECT_SET, quad_offset, NULL, hnquad, NULL);
  status = H5Sselect_hyperslab(dspace_file_ref_quad_id, H5S_SELECT_SET, quad_offset, NULL, hnquad, NULL);
  status = H5Sselect_hyperslab(dspace_file_qreq_id, H5S_SELECT_SET, &required_offset[3], NULL, &hnqreq, NULL);
  status = H5Sselect_hyperslab(dspace_file_qpar_id, H5S_SELECT_SET, &parallel_offset[3], NULL, &hnqpar, NULL);

  dset_id = H5Dcreate(grp_mesh_id, "Quadrilaterals", H5T_NATIVE_INT, dspace_file_quad_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_quad_id, dspace_file_quad_id, dxpl_id, pquad);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "Quadrilaterals", H5T_NATIVE_INT, dspace_file_ref_quad_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_ref_quad_id, dspace_file_ref_quad_id, dxpl_id, pquad_ref);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "RequiredQuadrilaterals", H5T_NATIVE_INT, dspace_file_qreq_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_qreq_id, dspace_file_qreq_id, dxpl_id, pqreq);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "ParallelQuadrilaterals", H5T_NATIVE_INT, dspace_file_qpar_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_qpar_id, dspace_file_qpar_id, dxpl_id, pqpar);
  H5Dclose(dset_id);

  free(pquad); pquad = NULL;
  free(pquad); pquad_ref = NULL;
  free(pqreq); pqreq = NULL;
  free(pqpar); pqpar = NULL;

  /* Tetrahedra */
  ptet = (int*) calloc(ne, 4 * sizeof(int));
  ptet_ref = (int*) calloc(ne, sizeof(int));
  ptetreq = (int*) calloc(nereq, sizeof(int));
  ptetpar = (int*) calloc(nepar, sizeof(int));

  reqcount = parcount = 0;

  if (ne) {
    ne = 0;
    for (int i = 0 ; i < mesh->ne ; i++) {
      pe = &mesh->tetra[i + 1];
      if (MG_EOK(pe)) {
        for (int j = 0 ; j < 4 ; j++) {
          ptet[4 * ne + j] = mesh->point[pe->v[j]].tmp + point_offset[0] - 1;
        }
      }
      ptet_ref[i] = pe->ref;
      if (pe->tag & MG_REQ)    ptetreq[reqcount++] = ne + tetra_offset[0];
      if (pe->tag & MG_PARBDY) ptetpar[parcount++] = ne + tetra_offset[0];
      ne++;
    }
  }

  status = H5Sselect_hyperslab(dspace_file_tetra_id, H5S_SELECT_SET, tetra_offset, NULL, hne, NULL);
  status = H5Sselect_hyperslab(dspace_file_ref_tetra_id, H5S_SELECT_SET, tetra_offset, NULL, hne, NULL);
  status = H5Sselect_hyperslab(dspace_file_tetreq_id, H5S_SELECT_SET, &required_offset[4], NULL, &hnereq, NULL);
  status = H5Sselect_hyperslab(dspace_file_tetpar_id, H5S_SELECT_SET, &parallel_offset[4], NULL, &hnepar, NULL);

  dset_id = H5Dcreate(grp_mesh_id, "Tetrahedra", H5T_NATIVE_INT, dspace_file_tetra_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_tetra_id, dspace_file_tetra_id, dxpl_id, ptet);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "TetrahedraRef", H5T_NATIVE_INT, dspace_file_ref_tetra_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_ref_tetra_id, dspace_file_ref_tetra_id, dxpl_id, ptet_ref);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "RequiredTetrahedra", H5T_NATIVE_INT, dspace_file_tetreq_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_tetreq_id, dspace_file_tetreq_id, dxpl_id, ptetreq);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "ParallelTetrahedra", H5T_NATIVE_INT, dspace_file_tetpar_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_tetpar_id, dspace_file_tetpar_id, dxpl_id, ptetpar);
  H5Dclose(dset_id);

  free(ptet); ptet = NULL;
  free(ptet); ptet_ref = NULL;
  free(ptetreq); ptetreq = NULL;
  free(ptetpar); ptetpar = NULL;

  /* Prisms */
  pprism = (int*) calloc(nprism, 6 * sizeof(int));
  pprism_ref = (int*) calloc(nprism, sizeof(int));
  if (nprism){
    for (int i = 0 ; i < mesh->nprism ; i++) {
      pp = &mesh->prism[i + 1];
      for (int j = 0 ; j < 6 ; j++) {
        pprism[6 * i + j] = mesh->point[pp->v[j]].tmp + point_offset[0] - 1;
      }
      pprism_ref[i] = pp->ref;
    }
  }

  status = H5Sselect_hyperslab(dspace_file_prism_id, H5S_SELECT_SET, prism_offset, NULL, hnprism, NULL);
  status = H5Sselect_hyperslab(dspace_file_ref_prism_id, H5S_SELECT_SET, prism_offset, NULL, hnprism, NULL);

  dset_id = H5Dcreate(grp_mesh_id, "Prisms", H5T_NATIVE_INT, dspace_file_prism_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_prism_id, dspace_file_prism_id, dxpl_id, pprism);
  H5Dclose(dset_id);

  dset_id = H5Dcreate(grp_mesh_id, "PrismsRef", H5T_NATIVE_INT, dspace_file_ref_prism_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(dset_id, H5T_NATIVE_INT, dspace_mem_ref_prism_id, dspace_file_ref_prism_id, dxpl_id, pprism_ref);
  H5Dclose(dset_id);

  free(pprism); pprism = NULL;
  free(pprism); pprism_ref = NULL;

  /*------------------------- END -------------------------*/

  /* Release the remaining HDF5 IDs (Dataspace, property lists, groups and the file) */
  status = H5Sclose(dspace_mem_point_id);
  status = H5Sclose(dspace_mem_edge_id);
  status = H5Sclose(dspace_mem_tria_id);
  status = H5Sclose(dspace_mem_quad_id);
  status = H5Sclose(dspace_mem_tetra_id);
  status = H5Sclose(dspace_mem_prism_id);

  status = H5Sclose(dspace_mem_ref_point_id);
  status = H5Sclose(dspace_mem_ref_edge_id);
  status = H5Sclose(dspace_mem_ref_tria_id);
  status = H5Sclose(dspace_mem_ref_quad_id);
  status = H5Sclose(dspace_mem_ref_tetra_id);
  status = H5Sclose(dspace_mem_ref_prism_id);

  status = H5Sclose(dspace_mem_corner_id);
  status = H5Sclose(dspace_mem_req_id);
  status = H5Sclose(dspace_mem_par_id);

  status = H5Sclose(dspace_mem_ridge_id);
  status = H5Sclose(dspace_mem_edreq_id);
  status = H5Sclose(dspace_mem_edpar_id);

  status = H5Sclose(dspace_mem_treq_id);
  status = H5Sclose(dspace_mem_tpar_id);

  status = H5Sclose(dspace_mem_qreq_id);
  status = H5Sclose(dspace_mem_qpar_id);

  status = H5Sclose(dspace_mem_tetreq_id);
  status = H5Sclose(dspace_mem_tetpar_id);

  status = H5Sclose(dspace_file_point_id);
  status = H5Sclose(dspace_file_edge_id);
  status = H5Sclose(dspace_file_tria_id);
  status = H5Sclose(dspace_file_quad_id);
  status = H5Sclose(dspace_file_tetra_id);
  status = H5Sclose(dspace_file_prism_id);

  status = H5Sclose(dspace_file_ref_point_id);
  status = H5Sclose(dspace_file_ref_edge_id);
  status = H5Sclose(dspace_file_ref_tria_id);
  status = H5Sclose(dspace_file_ref_quad_id);
  status = H5Sclose(dspace_file_ref_tetra_id);
  status = H5Sclose(dspace_file_ref_prism_id);

  status = H5Sclose(dspace_file_corner_id);
  status = H5Sclose(dspace_file_req_id);
  status = H5Sclose(dspace_file_par_id);

  status = H5Sclose(dspace_file_ridge_id);
  status = H5Sclose(dspace_file_edreq_id);
  status = H5Sclose(dspace_file_edpar_id);

  status = H5Sclose(dspace_file_treq_id);
  status = H5Sclose(dspace_file_tpar_id);

  status = H5Sclose(dspace_file_qreq_id);
  status = H5Sclose(dspace_file_qpar_id);

  status = H5Sclose(dspace_file_tetreq_id);
  status = H5Sclose(dspace_file_tetpar_id);

  status = H5Gclose(grp_mesh_id);
  status = H5Gclose(grp_sols_id);
  status = H5Fclose(file_id);
  status = H5Pclose(fapl_id);
  status = H5Pclose(dxpl_id);

  /* Free all allocated memory */
  free(nentities); nentities = NULL;

  return ier;
}
