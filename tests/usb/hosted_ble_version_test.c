#include <assert.h>
#include <stdio.h>
#include "hosted_ble_version.h"
int main(void)
{
    assert(!hosted_ble_controller_rpc(2,3,0));
    assert(!hosted_ble_controller_rpc(2,5,1));
    assert(hosted_ble_controller_rpc(2,5,2));
    assert(hosted_ble_controller_rpc(2,6,0));
    assert(hosted_ble_controller_rpc(2,11,7));
    assert(hosted_ble_controller_rpc(3,0,0));
    puts("PASS Hosted controller lifecycle boundary 2.5.2");
    return 0;
}
