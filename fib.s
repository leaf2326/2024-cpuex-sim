
movi sp, 1024
movi a0, 10 # fib(10)を計算することにする
call fib
ebreak
halt

fib:
	beq a0, x0, case0
	movi x5, 1
	beq a0, x5, case1
	# リンクレジスタとnをバックアップ
	subi sp, sp, 12
	sw ra, 8(sp)
	sw a0, 4(sp)
	# fib(n - 1)
	subi a0, a0, 1
	call fib
	sw a0, 0(sp) # 結果をバックアップ
	# fib(n - 2)
	lw a0, 4(sp)
	subi a0, a0, 2
	call fib
	# 2つの結果を足す
	lw a1, 0(sp)
	add a0, a0, a1
	lw ra, 8(sp) # ra戻す
	addi sp, sp, 12
	ret
	case0: # n = 0
	movi a0, 0
	ret
	case1: # n = 1
	movi a0, 1
	ret
