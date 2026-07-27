# Model composition as independently resolved segments

Meld models an active composition as settled language segments plus one
pending raw segment, rather than keeping all keystrokes in one librime
composition and patching its prefix and suffix afterward. This costs more
explicit state management, but it prevents later English or partial-Zhuyin
parsing from reverting resolved Chinese, allows phrase ranking without
freezing the whole sentence, and gives deletion, mode switching, and manual
candidate pinning one consistent unit of behavior.
