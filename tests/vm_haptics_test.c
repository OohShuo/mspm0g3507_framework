#include <assert.h>

#include "haptics_vm.h"

int main(void) {
    Vm_Haptics_Set_Strength(25u);
    assert(Vm_Haptics_Get_Strength() == 25u);
    Vm_Haptics_Set_Strength(150u);
    assert(Vm_Haptics_Get_Strength() == 100u);
    Vm_Haptics_Set_Strength(0u);
    assert(Vm_Haptics_Get_Strength() == 0u);
    return 0;
}
