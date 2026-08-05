#include "functionalpass.h"

void FunctionalPass::Create(void) {

}
void FunctionalPass::Destroy(void) {

};
void FunctionalPass::Execute(SDL_GPUCommandBuffer* cmd) {
    function(cmd);
};