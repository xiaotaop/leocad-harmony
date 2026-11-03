#include <cstdio>
#define main D7FontProbeMain
#include "../../tools/d7/tst_d7_fonts.cpp"
#undef main
int main(int argc, char** argv)
{
 if (!freopen("/data/storage/el2/base/files/d7-fonts.txt", "w", stdout)) return 2;
 return D7FontProbeMain(argc, argv);
}
