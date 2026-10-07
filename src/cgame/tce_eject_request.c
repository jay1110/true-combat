#include "tce_eject_request.h"
int TCE_CG_EjectRequest(int bolt, int singleReload, int eventParm,
    int hasCallback, int brassTime, int pending) {
    if (bolt || (singleReload && !eventParm) || !hasCallback || brassTime<1)
        return pending;
    return 1;
}
