/* Actual math/spline implementations are compiled by the runtime runner.
 * Only the fatal engine boundary remains an aborting service. */
#include "tce_trajectory.h"
void QDECL Com_Error(int code,const char *format,...) {(void)code;(void)format;abort();}
void BG_EvaluateTrajectory(const trajectory_t *trajectory,int time,vec3_t out,qboolean angle,int path)
{TCE_BG_EvaluateTrajectory(trajectory,time,out,angle,path,1);}
