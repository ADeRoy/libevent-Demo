#include "LibeventSvr.h"
int main()
{
    MasageSvr svr;
    svr.Start("0.0.0.0", 8000);
    return 0;
}