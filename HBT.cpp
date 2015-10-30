using namespace std;
#include <iostream>
#include <string>
#include <cstdlib>
#include <chrono>

#include "simulation_io/snapshot.h"
#include "simulation_io/halo.h"
#include "simulation_io/subhalo.h"
#include "mymath.h"

int main(int argc, char **argv)
{
#ifdef _OPENMP
 omp_set_nested(0);
#endif
  int snapshot_start, snapshot_end;
  ParseHBTParams(argc, argv, HBTConfig, snapshot_start, snapshot_end);
  mkdir(HBTConfig.SubhaloPath.c_str(), 0755);
  MarkHBTVersion();
	
  SubhaloSnapshot_t subsnap;
  
  subsnap.Load(snapshot_start-1, true);
  
  Timer_t timer;
  ofstream time_log(HBTConfig.SubhaloPath+"/timing.log", fstream::out|fstream::app);
  time_log<<fixed<<setprecision(1);//<<setw(8);
  for(int isnap=snapshot_start;isnap<=snapshot_end;isnap++)
  {
	timer.Tick();
	ParticleSnapshot_t partsnap;
	partsnap.Load(isnap);
	subsnap.SetSnapshotIndex(isnap);
	
	HaloSnapshot_t halosnap;
	halosnap.Load(isnap);
	timer.Tick();
	#pragma omp parallel
	{
	halosnap.ParticleIdToIndex(partsnap);
	subsnap.ParticleIdToIndex(partsnap);
	#pragma omp master
	timer.Tick();
	subsnap.AssignHosts(halosnap);
	subsnap.PrepareCentrals(halosnap);
	}

	timer.Tick();
	subsnap.RefineParticles();
	timer.Tick();
	
	#pragma omp parallel
	{
	subsnap.UpdateTracks();
	subsnap.ParticleIndexToId();
	}
	timer.Tick();
	
	subsnap.Save();
	
	timer.Tick();
	time_log<<isnap;
	for(int i=0;i<timer.Size()-1;i++)
	  time_log<<"\t"<<timer.GetSeconds(i);
	time_log<<endl;
	timer.Reset();
  }
  
  return 0;
}