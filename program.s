_start:
    lw x1, 10(x0)
    addi x2, x0, 0  
    addi x3, x0, 1   
    addi x4, x0, 1   

    beq x1, x0, done  
    beq x1, x4, done 

fib_loop:
    add x5, x2, x3

    addi x2, x3, 0     # F(n-2) = F(n-1)
    addi x3, x5, 0     # F(n-1) = F(i)

    addi x4, x4, 1

    bne x4, x1, fib_loop

done:
    sw x3, 0(x0)

    jalr x0, x0, 0
