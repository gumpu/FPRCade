

# To show hex values
define xxd_hex
  set $addr = $arg0
  set $n = $arg1
  set $end = $addr + ($n * 4)
  while $addr < $end
    # printf "%08x: ", $addr # Removed this line
    x/4wx $addr
    set $addr += 16
  end
end

# To show ASCII values
define xxd_ascii
  set $addr = $arg0
  set $n = $arg1
  set $end = $addr + ($n * 4)
  while $addr < $end
    set $byte = *(char*)$addr
    if $byte >= 32 && $byte < 127
      printf "%c", $byte
    else
      printf "."
    end
    set $addr += 1
  end
  printf "\n"
end

define xxd
  set $addr = $arg0
  set $n = $arg1
  set $end = $addr + ($n * 4)
  while $addr < $end
    printf "0x%016llx: ", (unsigned long long)$addr
    set $hex_end = $addr + 16
    set $tmp_addr = $addr
    while $tmp_addr < $hex_end
      printf "%02x ", *(unsigned char*)$tmp_addr
      set $tmp_addr += 1
    end
    printf " "
    set $tmp_addr = $addr
    while $tmp_addr < $hex_end
      set $byte = *(char*)$tmp_addr
      if $byte >= 32 && $byte < 127
        printf "%c", $byte
      else
        printf "."
      end
      set $tmp_addr += 1
    end
    printf "\n"
    set $addr += 16
  end
end

file ./fpemu
set args test_001.hex
break main
break run
run

tui enable

