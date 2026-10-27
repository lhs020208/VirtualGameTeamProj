#include "DEMSolver.h"
#include <iostream>
int main()
{
    dem::Solver s;
    dem::Config c;
    s.reset(c);
    double g = 0, b = 0, f = 0, i = 0, t = 0;
    for (int n = 0; n < 6000; ++n)
    {
        s.step();
        if (n >= 1000)
        {
            g += s.s.gridMs;
            b += s.s.searchMs;
            f += s.s.contactMs;
            i += s.s.integrateMs;
            t += s.s.statsMs;
        }
    }
    std::cout << "grid_ms,search_ms,contact_ms,integration_ms,stats_ms\n"
              << g / 5000 << "," << b / 5000 << "," << f / 5000 << "," << i / 5000 << "," << t / 5000 << "\n";
}
