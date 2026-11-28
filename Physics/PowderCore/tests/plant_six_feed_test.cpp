#include "powder/plant.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("expected plant data directory");
        const std::string dir(argv[1]);
        auto data = powder::read_plant_data(dir + "/runtime.plant");
        auto routes = powder::read_plant_routes(dir + "/runtime.routes");
        const auto gates = powder::plant_supplier_gates(data);
        if (gates.size() != 6 || routes.size() != 7) throw std::runtime_error("six feed routes expected");
        std::vector<powder::Grain> points;
        for (int index : gates) {
            auto gate = data[index];
            points.push_back({gate.x,gate.y,gate.z+.001f,0});
        }
        powder::GranularConfig cfg;
        cfg.plant=true; cfg.plant_threaded=true; cfg.kernel_optimize=true;
        cfg.plant_wall_mode=5; cfg.plant_data=data; cfg.count=6;
        cfg.radius=.002f; cfg.dt=.00001f; cfg.iterations=8;
        powder::GranularGPU gpu(cfg,points,std::vector<powder::Grain>(6));
        for (unsigned branch=0; branch<6; ++branch) {
            unsigned mask=1u<<branch;
            powder::plant_set_manual_valves(data,routes,1000,mask,mask,true,true);
            gpu.update_plant_field(data);
            auto hits=gpu.inspect_boundaries(points,0);
            for (unsigned i=0; i<6; ++i) {
                double flow=data[routes[i].cells.front()+2].x;
                bool open=i==branch;
                if ((open && flow<=0) || (!open && flow!=0))
                    throw std::runtime_error("unexpected branch flow");
                double shift=hits[i].position.z-points[i].z;
                if (open ? std::abs(shift)>1.e-6 : shift<.0038)
                    throw std::runtime_error("incorrect open/closed gate response");
            }
            std::cout << char('A'+branch) << " " << data[routes[branch].cells.front()+2].x << " m3/s PASS\n";
        }
    } catch(const std::exception& e) {
        std::cerr << e.what() << "\n"; return 1;
    }
}
