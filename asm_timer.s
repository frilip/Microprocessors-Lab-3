.global delay_cycles
    .p2align 2
    .type delay_cycles,%function

delay_cycles:
    .fnstart
    LSRS r1, r0, #2       // r1 = iterations = r0 / 4
    BEQ done
    MOV r2, r1            // counter for loop
loop:
    SUBS r2, #1
#if __CORTEX_M == 3 || __CORTEX_M == 4
    NOP                   // 1 cycle
#endif
    BNE loop
    // Estimate: 3 cycles per iteration
    MOV r0, r1            // r0 = number of iterations
    MOV r2, #3
    MUL r0, r0, r2        // r0 = r1 * 3 = estimated cycles
done:
    BX lr
    .fnend