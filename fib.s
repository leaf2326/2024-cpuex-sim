_start:
    addi x1, x0, -12
    addi x2, x0, -12  
    addi x3, x0, 10
    addi x4, x0, 10
    addi x5, x0, 100
loop:
    addi x4, x4, 10
    bne x0, x1, done
    beq x1, x2, loop  
    bne x1, x4, -4
    bne x3, x2, -4
    beq x1, x2, -4
done:
    bne x5, x4, -4
    addi x5, x0, -100
    ebreak
    

