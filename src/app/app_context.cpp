#include "app_context.h"

AppContext &AppContext::instance()
{
    static AppContext inst;
    return inst;
}
