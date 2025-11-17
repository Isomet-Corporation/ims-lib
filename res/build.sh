rm -f imshw.h
rm -f settings_ini.h
xxd -i imshw.db > "imshw.h"
xxd -i settings.ini > "settings_ini.h"
