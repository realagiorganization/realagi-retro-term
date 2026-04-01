#ifndef YMFM_VGMRENDER_H
#define YMFM_VGMRENDER_H

#include <atomic>

int ymfm_vgmrender_file(const char *filename,
                        const char *outfilename,
                        int output_rate,
                        std::atomic_bool *cancel_flag = nullptr);

#endif // YMFM_VGMRENDER_H
