#include "model.h"

namespace model {

State& state() {
    static State s;
    return s;
}

}  // namespace model
