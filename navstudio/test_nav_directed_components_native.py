"""Actual iterative native SCC selector, including separate spawn components."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase,main,skipUnless

@skipUnless(os.name=="nt" and shutil.which("cmake"),"native MSVC fixture needs Windows/CMake")
class DirectedComponents(TestCase):
 def test_current_edges_not_union_of_all_spawns_select_fronts(self):
  header=((Path(__file__).resolve().parents[1] / "src")/"game/tce_nav_graph.h").as_posix()
  program='#include <stdio.h>\n#include "HEADER"\nint main(void) {\n unsigned char active[5]={1,1,1,1,1};int groups[5],i;\n tce_navGraphEdge edges[7]={{0,1},{1,0},{1,2},{2,3},{3,4},{4,3},{4,0}};\n /* All five may be reached from their own spawns. An incoming-only\n  * transition still does not give the upper spawning terrace a return. */\n if(TCE_NavStrongComponents(5,6,active,edges,groups)!=3)return 1;\n if(groups[0]!=groups[1]||groups[3]!=groups[4]||groups[1]==groups[3]||groups[2]==groups[0]||groups[2]==groups[3])return 2;\n if(TCE_NavStrongComponents(5,7,active,edges,groups)!=1)return 3;\n active[4]=0;if(TCE_NavStrongComponents(5,6,active,edges,groups)!=3||groups[4]!=-1)return 4;\n edges[0].to=99;if(TCE_NavStrongComponents(5,1,active,edges,groups)!=-1)return 5;\n {int count=16384;tce_navGraphEdge *large=malloc(count*sizeof(*large));unsigned char *mask=malloc(count);int *g=malloc(count*sizeof(int));\n  if(!large||!mask||!g)return 6;\n  for(i=0;i<count;i++){mask[i]=1;large[i].from=i;large[i].to=(i+1)%count;}\n  if(TCE_NavStrongComponents(count,count,mask,large,g)!=1)return 7;\n  for(i=1;i<count;i++)if(g[i]!=g[0])return 8;\n  free(large);free(mask);free(g);\n }\n puts("native current directed SCC/spawn-island controls passed");return 0;\n}\n'.replace("HEADER",header)
  with tempfile.TemporaryDirectory(prefix="tce-directed-graph-") as folder:
   root=Path(folder);(root/"main.c").write_text(program)
   (root/"CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.15)\nproject(NavDirected LANGUAGES C)\nadd_executable(nav-directed main.c)\n")
   for command in (["cmake","-S",folder,"-B",str(root/"build"),"-A","Win32"],["cmake","--build",str(root/"build"),"--config","Release"],[str(root/"build/Release/nav-directed.exe")]):
    r=subprocess.run(command,capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
    self.assertEqual(r.returncode,0,r.stdout+r.stderr)

if __name__=="__main__":main()
