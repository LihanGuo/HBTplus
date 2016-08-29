#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <cstring>
#include <omp.h>

#include "../../snapshot.h"
#include "JING_IO.h"

extern int alloc_file_unit_(int *reset);
extern void open_fortran_file_(char *filename,int *fileno,int *endian,int *error);
extern void skip_fortran_record_(int *fileno);
extern void read_fortran_record1_(void *arr,long int *arr_len,int *fileno);
extern void read_fortran_record2_(void *arr,long int *arr_len,int *fileno);
extern void read_fortran_record4_(void *arr,long int *arr_len,int *fileno);
extern void read_fortran_record8_(void *arr,long int *arr_len,int *fileno);
extern void close_fortran_file_(int *fileno);
extern void read_part_arr_imajor_(float partarr[][3],long int *np,int *fileno);//old format
extern void read_part_arr_xmajor_(float partarr[][3],long int *np,int *fileno);//newest format
extern void read_part_header_int4_(int *np,int *ips,float *ztp,float *omgt,float *lbdt,
			      float *boxsize,float *xscale,float *vscale,int *fileno);
extern void read_part_header_int8_(long int *np,long int *ips,float *ztp,float *omgt,float *lbdt,
			      float *boxsize,float *xscale,float *vscale,int *fileno);
extern void read_group_header_int4_(float *b,int *ngrp, int *fileno);
extern void read_group_header_int8_(float *b,long int *ngrp, int *fileno);

//Caution:: only support SAME_INTTYPE and SAME_REALTYPE; if they differ, need further modification of code!
#ifdef HBT_INT8
#define read_fortran_record_HBTInt read_fortran_record8_
#define read_part_header read_part_header_int8_
#define read_group_header read_group_header_int8_
#else
#define read_fortran_record_HBTInt read_fortran_record4_
#define read_part_header read_part_header_int4_
#define read_group_header read_group_header_int4_
#endif

#ifdef PDAT_XMAJOR
#define read_part_arr read_part_arr_xmajor_
#else
#define read_part_arr read_part_arr_imajor_
#endif

void JingReader_t::ReadIdFileSingle(int ifile, vector< Particle_t >& Particles)
{
  long int nread=Header.Np/NumFilesId;
  string filename=GetFileName("id", ifile);
  vector <HBTInt> PId(nread);
  
  int fileno, filestat, flag_endian=NeedByteSwap;
  open_fortran_file_(filename.c_str(),&fileno,&flag_endian,&filestat);
  if(filestat) throw(runtime_error("failed to open file "+filename+", error no. "+to_string(filestat)+"\n"));
  read_fortran_record_HBTInt(PId.data(), &nread, &fileno);
  close_fortran_file_(&fileno);
  
  auto curr_particles=Particles.data()+nread*ifile;
  for(HBTInt i=0;i<nread;i++)
	curr_particles[i].Id=PId[i];
}
void JingReader_t::ReadId(vector <Particle_t> &Particles)
{
  if(HBTConfig.SnapshotHasIdBlock)
  {
    for(int ifile=0;ifile<NumFilesId;ifile++)
    #pragma omp task firstprivate(ifile)
      ReadIdFileSingle(ifile, Particles);
  }
  else
  {
    #pragma omp task
    for(HBTInt i=0;i<Particles.size();i++)
      Particles[i].Id=i;
  }
}
void JingReader_t::CheckIdRange(vector <Particle_t>&Particles)
{
  HBTInt i;
  #pragma omp parallel for
  for(i=0;i<Header.Np;i++)
    if(Particles[i].Id<1||Particles[i].Id>Header.Np)
      throw(runtime_error("id not in the range 1~"+to_string(Header.Np)+", for i="+to_string(i)+",pid="+to_string(Particles[i].Id)+", snap="+to_string(SnapshotId)+endl));
}
void JingReader_t::ReadPosFileSingle(int ifile, vector< Particle_t >& Particles)
{
  long int nread=Header.Np/NumFilesId;
  string filename=GetFileName("pos", ifile);
  auto curr_particles=Particles.data()+nread*ifile;
  
  int fileno,filestat, flag_endian=NeedByteSwap;
  open_fortran_file_(filename.c_str(),&fileno,&flag_endian,&filestat);
  if(filestat) throw(runtime_error("failed to open file "+filename+", error no. "+to_string(filestat)+"\n"));
	
  if(0==ifile)	skip_fortran_record_(&fileno);
	
  if(Header.FlagHasScale)//has scale
  {
    assert(sizeof(short)==2);
    vector <short> tmp(nread);
    for(int i=0;i<3;i++)
    {
      read_fortran_record2_(tmp.data(),&nread,&fileno);
      for(HBTInt j=0;j<nread;j++)
	      curr_particles[j].ComovingPosition[i]=tmp[j]*Header.xscale;
    }
  }
  else
  {
    vector <HBTxyz> Pos(nread);
    read_part_arr(Pos.data(),&nread,&fileno);
    for(HBTInt j=0;j<nread;j++)
      curr_particles[j].ComovingPosition=Pos[j];
  }
  close_fortran_file_(&fileno);
}
void JingReader_t::ReadVelFileSingle(int ifile, vector< Particle_t >& Particles)
{
  long int nread=Header.Np/NumFilesId;
  string filename=GetFileName("vel", ifile);
  auto curr_particles=Particles.data()+nread*ifile;
  
  int fileno,filestat, flag_endian=NeedByteSwap;
  open_fortran_file_(filename.c_str(),&fileno,&flag_endian,&filestat);
  if(filestat) throw(runtime_error("failed to open file "+filename+", error no. "+to_string(filestat)+"\n"));
	
  if(0==ifile)	skip_fortran_record_(&fileno);
	
  if(Header.FlagHasScale)//has scale
  {
    assert(sizeof(short)==2);
    vector <short> tmp(nread);
    for(int i=0;i<3;i++)
    {
      read_fortran_record2_(tmp.data(),&nread,&fileno);
      for(HBTInt j=0;j<nread;j++)
	      curr_particles[j].PhysicalVelocity[i]=tmp[j]*Header.vscale;
    }
  }
  else
  {
    vector <HBTxyz> Pos(nread);
    read_part_arr(Pos.data(),&nread,&fileno);
    for(HBTInt j=0;j<nread;j++)
      curr_particles[j].PhysicalVelocity=Pos[j];
  }
  close_fortran_file_(&fileno);
}
void JingReader_t::ReadPosition(vector <Particle_t> &Particles)
{
	for(int ifile=0;ifile<NumFilesPos;ifile++)
  	#pragma omp task firstprivate(ifile)
	  ReadPosFileSingle(ifile, Particles);
}
void JingReader_t::ReadVelocity(vector <Particle_t> &Particles)
{
	for(int ifile=0;ifile<NumFilesVel;ifile++)
  	#pragma omp task firstprivate(ifile)
	  ReadVelFileSingle(ifile, Particles);
}
string JingReader_t::GetFileName(const char * filetype, int iFile)
{
  char buf[1024];
  if(iFile==0)
  sprintf(buf,"%s/%s%s.%04d", HBTConfig.SnapshotPath.c_str(), filetype, HBTConfig.SnapshotFileBase.c_str(),SnapshotId);
  if(!file_exist(buf))  sprintf(buf,"%s/%s%s.%04d.%02d",HBTConfig.SnapshotPath.c_str(), filetype, HBTConfig.SnapshotFileBase.c_str(),SnapshotId, iFile+1);
  return string(buf);
}

void JingReader_t::ProbeFiles()
{ 
  int flag_endian=NeedByteSwap,filestat,fileno;
  int reset_fileno=1;
  alloc_file_unit_(&reset_fileno);//reset fortran fileno pool 
  
  string filename=GetFileName("pos");
  open_fortran_file_(filename.c_str(),&fileno,&flag_endian,&filestat);
  if(filestat)
  {
    cerr<<"Error opening file "<<filename<<",error no. "<<filestat<<endl;
    exit(1);
  }

  JingHeader_t header;
  read_part_header(&header.Np,&header.ips,&header.Redshift,&header.Omegat,&header.Lambdat,
					  &header.BoxSize,&header.xscale,&header.vscale,&fileno);
  if(header.BoxSize!=HBTConfig.BoxSize)
  {
    NeedByteSwap=!NeedByteSwap;
    auto L=header.BoxSize;
    swap_Nbyte(&L, 1, sizeof(L));
    if(L!=HBTConfig.BoxSize)
    {
      cerr<<"Error: boxsize check failed. maybe wrong unit? expect "<<HBTConfig.BoxSize<<", found "<<header.BoxSize<<" (or "<<L<<" )"<<endl;
      exit(1);
    }
  }

  close_fortran_file_(&fileno);

  NumFilesId=CountFiles("id");
  NumFilesPos=CountFiles("pos");
  NumFilesVel=CountFiles("vel");
}

int JingReader_t::CountFiles(const char *filetype)
{
  char filename[1024], pattern[1024], basefmt[1024], fmt[1024];
  const int ifile=1;
  sprintf(basefmt,"%s/%%s%s.%04d",HBTConfig.SnapshotPath.c_str(), HBTConfig.SnapshotFileBase.c_str(), SnapshotId);
  sprintf(basefmt, "%s/groups_%03d/subhalo_%%s_%03d",HBTConfig.HaloPath.c_str(),SnapshotId,SnapshotId);
  sprintf(fmt, "%s.%%02d", basefmt);
  sprintf(filename, fmt, filetype, ifile);
  int nfiles=1;
  if(file_exist(filename))
  {
	sprintf(pattern, basefmt, filetype);
	strcat(pattern, ".*");
	nfiles=count_pattern_files(pattern);
  }
  return nfiles;
}

void JingReader_t::ReadHeader(JingHeader_t& header, const char *filetype, int ifile)
{	
  short *tmp;
  int flag_endian=NeedByteSwap,filestat,fileno;
  int reset_fileno=1;
  alloc_file_unit_(&reset_fileno);//reset fortran fileno pool 
  
  string filename=GetFileName(filetype, ifile);
  open_fortran_file_(filename.c_str(),&fileno,&flag_endian,&filestat);
  if(filestat)
  {
    cerr<<"Error opening file "<<filename<<",error no. "<<filestat<<endl;
    exit(1);
  }
  read_part_header(&header.Np,&header.ips,&header.Redshift,&header.Omegat,&header.Lambdat,
		   &header.BoxSize,&header.xscale,&header.vscale,&fileno);
  close_fortran_file_(&fileno);
  assert(header.BoxSize==HBTConfig.BoxSize);
  
  LoadExtraHeaderParams(header);
  header.mass[0]=0.;
  header.mass[1]=header.OmegaM0*3.*PhysicalConst::H0*PhysicalConst::H0/8./3.1415926/PhysicalConst::G*header.BoxSize*header.BoxSize*header.BoxSize/header.Np;//particle mass in units of 10^10Msun/h

  cout<<"z="<<header.Redshift<<endl;
    
  float Hratio; //(Hz/H0)
  float scale_reduced,scale0;//a,R0
  Hratio=sqrt(header.OmegaLambda0/header.Lambdat);
  header.Hz=PhysicalConst::H0*Hratio;
  scale_reduced=1./(1.+header.Redshift);
  header.ScaleFactor=scale_reduced;
  scale0=1+header.RedshiftIni;//scale_INI=1,scale_reduced_INI=1./(1.+z_ini),so scale0=scale_INI/scale_reduce_INI;
  header.vunit=100.*header.BoxSize*Hratio*scale_reduced*scale_reduced*scale0;   /*vunit=100*rLbox*R*(H*R)/(H0*R0)
  =L*H0*Hratio*R*R/R0 (H0=100 when length(L) in Mpc/h)
  *      =100*L*(H/H0)*a*a*R0
  * where a=R/R0;         */
}

void JingReader_t::LoadExtraHeaderParams(JingHeader_t &header)
{
  stringstream filename;
  filename<<HBTConfig.SnapshotPath<<"/"<<"Parameters.txt";
  ifstream ifs;
  ifs.open(filename.str());
  if(!ifs.is_open())
  {
	cerr<<"Error opening config file "<<filename.str()<<endl;
	exit(1);
  }
  string name;
  HBTReal OmegaM0, OmegaL0, RedshiftIni;
  HBTInt SnapDivScale;
#define ReadNameValue(x) ifs<<name<<x;assert(name==#x)
  ReadNameValue(OmegaM0);
  ReadNameValue(OmegaL0);
  ReadNameValue(RedshiftIni);
  ReadNameValue(SnapDivScale);
#undef ReadNameValue
  ifs.close();
  header.OmegaM0=OmegaM0;
  header.OmegaLambda0=OmegaL0;
  header.RedshiftIni=RedshiftIni;
  header.SnapDivScale=SnapDivScale;
  header.FlagHasScale=(SnapshotId<=header.SnapDivScale);
}

void JingReader_t::LoadSnapshot(vector <Particle_t> &Particles, Cosmology_t &Cosmology)
{
  ReadHeader(Header);
  Cosmology.Set(Header.ScaleFactor, Header.OmegaM0, Header.OmegaLambda0);
#ifdef DM_ONLY
  Cosmology.ParticleMass=Header.mass[TypeDM];
#endif
  Particles.resize(Header.Np);
  
  int nthreads=NumFilesId+NumFilesPos+NumFilesVel;
  #pragma omp parallel num_threads(nthreads)
  #pragma omp single //nowait //creating tasks inside
  {
    ReadPosition(Particles);
    ReadVelocity(Particles);
    ReadId(Particles);
  }
// 	#pragma omp taskwait
	
  if(HBTConfig.SnapshotHasIdBlock)  CheckIdRange(Particles);
  
  #pragma omp parallel for
  for(HBTInt i=0;i<Header.Np;i++)
  {
    auto &p=Particles[i];
    for(int j=0;j<3;j++)
    {
      p.ComovingPosition[j]-=floor(p.ComovingPosition[j]);	//format coordinates to be in the range [0,1)
      p.ComovingPosition[j]*=Header.BoxSize;			//comoving coordinate in units of kpc/h
      p.PhysicalVelocity[j]*=Header.vunit;			//physical peculiar velocity in units of km/s
    }
  }
	
  cout<<" ( "<<nthreads<<" total files ) : "<<Particles.size()<<" particles loaded."<<endl;
}

namespace JingGroup
{
  int ProbeGroupFileByteOrder(int snapshot_id)
  {
    int flag_endian=false, filestat, fileno;
    char buf[1024];
    sprintf(buf, "%s/fof.b20.%s.%04d",HBTConfig.HaloPath.c_str(), HBTConfig.SnapshotFileBase, snapshot_id);
    open_fortran_file_(buf,&fileno,&flag_endian,&filestat);
    if(filestat) throw(runtime_error("failed to open file "+buf+", error no. "+to_string(filestat)+endl));
    float b;
    HBTInt Ngroups;
    read_group_header(&b,&Ngroups,&fileno);
    close_fortran_file_(&fileno);
    if(b!=0.2)
    {
      flag_endian=true;
      assert(swap_Nbyte(&b, 1, sizeof(b))==0.2);
    }
    return flag_endian;
  }
  void LoadGroup(int snapshot_id, vector< Halo_t >& Halos)
  {
    long int nread;
	  
    int flag_endian=ProbeGroupFileByteOrder(snapshot_id), filestat, fileno;
    char buf[1024];
    sprintf(buf, "%s/fof.b20.%s.%04d",HBTConfig.HaloPath.c_str(), HBTConfig.SnapshotFileBase, snapshot_id);
    open_fortran_file_(buf,&fileno,&flag_endian,&filestat);
    if(filestat) throw(runtime_error("failed to open file "+buf+", error no. "+to_string(filestat)+endl));
    float b;
    HBTInt Ngroups;
    read_group_header(&b,&Ngroups,&fileno);
    Halos.resize(Ngroups);
    
    for(int i=0;i<3;i++)
      skip_fortran_record_(&fileno);
  //   vector <float> HaloCenX(Ngroups), HaloCenY(Ngroups), HaloCenZ(Ngroups);
  //   nread=Ngroups;
  //   read_fortran_record4_(HaloCenX.data(), &nread, &fileno);
  //   read_fortran_record4_(HaloCenY.data(), &nread, &fileno);
  //   read_fortran_record4_(HaloCenZ.data(), &nread, &fileno);
  //   for(HBTInt i=0;i<Ngroups;i++)
  //   {
  //     HaloCenX[i]*=HBTConfig.BoxSize;
  //     HaloCenY[i]*=HBTConfig.BoxSize;
  //     HaloCenZ[i]*=HBTConfig.BoxSize;
  //   }
  
    vector <HBTInt> Len(Ngroups), Offset(Ngroups);
    HBTInt Nids=0;
    for(HBTInt i=0;i<Ngroups;i++)
    {  
      nread=1;
      read_fortran_record_HBTInt(&Len[i],&nread,&fileno); 
      
      Offset[i]=Nids;
      Nids+=Len[i];
	  
      if(i>0&&Len[i]>Len[i-1]) 
	throw(runtime_error("Group size not ordered or wrong file format? group "+to_string(i)+", size "+to_string(Len[i])+", "+to_string(Len[i+1])+endl));
	  
      nread=Len[i];
      Halos[i].Particles.resize(Len[i]);
      read_fortran_record_HBTInt(Halos[i].Particles.data(),&nread,&fileno); 
    }
  
    close_fortran_file_(&fileno);
    
    cout<<"Snap="<<snapshot_id<<", Ngroups="<<Ngroups<<", Nids="<<Nids<<", b="<<b<<endl;
    
    if(HBTConfig.GroupLoadedIndex)
    #pragma omp parallel for
    for(HBTInt i=0;i<Ngroups;i++)
    {
	auto &particles=Halos[i].Particles;
	for(auto && p: particles)
	  p--;//change from [1,NP] to [0,NP-1] for index in C
    }
  }
};

