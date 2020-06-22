#include "process.h"

// app_printf
//     A version of console_printf that picks a sensible color by process ID.

void app_printf(int colorid, const char* format, ...) {
    int color;
    if (colorid < 0) {
        color = 0x0700;
    } else {
        static const uint8_t col[] = { 0x0E, 0x0F, 0x0C, 0x0A, 0x09 };
        color = col[colorid % sizeof(col)] << 8;
    }

    va_list val;
    va_start(val, format);
    cursorpos = console_vprintf(cursorpos, color, format, val);
    va_end(val);

    if (CROW(cursorpos) >= 23) {
        cursorpos = CPOS(0, 0);
    }
}


static inline int sys_read_serial(char * str) {
    int result = 0;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_READ_SERIAL), "D" /* %rdi */ (str)
                  : "cc", "memory");
    return result;
}

// read_line
// str should be at least max_chars + 1 byte
int read_line(char * str, int max_chars){
    static char cache[128];
    static int index = 0;
    static int length = 0;

    if(max_chars == 0){
        str[max_chars] = '\0';
        return 0;
    }
    str[max_chars + 1] = '\0';

    if(index < length){
        // some cache left
        int i = 0;
        for(i = index;
                i < length && (i - index + 1 < max_chars);
                i++){
            if(cache[i] == '\n'){
                // temporarily set cache[i] = '\0'
                cache[i] = '\0';
                cache[i] = '\n';
                memcpy(str, cache + index, i - index + 1);
                str[i-index+1] = '\0';
                //app_printf(1, "[%d, %d]-> %sxx", index, i, str);
                int len = i - index + 1;
                index = i + 1;
                return len;
            }
        }
        if(max_chars <= length - index + 1){
            // copy max_chars - 1 bytes and return
            memcpy(str, cache + index, max_chars);
            str[max_chars] = '\0';
            //app_printf(1, "[%d, %d]-> %sxx", index, index + max_chars - 1, str);
            index += max_chars;
            return max_chars;
        }
        else{
            // copy over available data, and start reading again
            memcpy(str, cache + index, length - index);
            //app_printf(1, "[%d, %d]->xx%sxx", index, length - 1, str);
            str += length - index;
            max_chars -= length - index;
            int len = length - index;
            index = length;
            len += read_line(str, max_chars);
            return len;
        }
    }
    else {
        // no cache left
        // reset read to 0
        index = 0;
        length = sys_read_serial(cache);
        if(length <= 0){
            str[0] = '\0';
            return 0;
        }
        return read_line(str, max_chars);
    }
    return 0;
}

// panic, assert_fail
//     Call the INT_SYS_PANIC system call so the kernel loops until Control-C.

void panic(const char* format, ...) {
    va_list val;
    va_start(val, format);
    char buf[160];
    memcpy(buf, "PANIC: ", 7);
    int len = vsnprintf(&buf[7], sizeof(buf) - 7, format, val) + 7;
    va_end(val);
    if (len > 0 && buf[len - 1] != '\n') {
        strcpy(buf + len - (len == (int) sizeof(buf) - 1), "\n");
    }
    (void) console_printf(CPOS(23, 0), 0xC000, "%s", buf);
    sys_panic(buf);
 spinloop: goto spinloop;       // should never get here
}

void assert_fail(const char* file, int line, const char* msg) {
    (void) console_printf(CPOS(23, 0), 0xC000,
                          "PANIC: %s:%d: assertion '%s' failed\n",
                          file, line, msg);
    sys_panic(NULL);
 spinloop: goto spinloop;       // should never get here
}
