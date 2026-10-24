from bytecode import BytecodeFunction, OpCode

class VM:
    def __init__(self):
        self.stack = []
        self.locals = []

    def run(self, fn: BytecodeFunction, args: list) -> int:
        # Initialiser les variables locales avec les arguments passés
        self.locals = list(args) + [0] * (fn.num_locals - len(args))
        self.stack.clear()
        
        ip = 0  # Instruction Pointer
        code_len = len(fn.code)

        while ip < code_len:
            inst = fn.code[ip]
            ip += 1

            op = inst.op
            arg = inst.arg

            if op == OpCode.OP_CONST:
                self.stack.append(fn.constants[arg])

            elif op == OpCode.OP_LOAD:
                self.stack.append(self.locals[arg])

            elif op == OpCode.OP_STORE:
                self.locals[arg] = self.stack.pop()

            elif op == OpCode.OP_ADD:
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(a + b)

            elif op == OpCode.OP_SUB:
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(a - b)

            elif op == OpCode.OP_MUL:
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(a * b)

            elif op == OpCode.OP_DIV:
                b = self.stack.pop()
                a = self.stack.pop()
                if b == 0:
                    raise ZeroDivisionError("Division par zéro dans la VM")
                self.stack.append(a // b)

            elif op == OpCode.OP_LT:
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a < b else 0)

            elif op == OpCode.OP_EQ:
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a == b else 0)

            elif op == OpCode.OP_JUMP:
                ip = arg

            elif op == OpCode.OP_JUMP_IF_FALSE:
                cond = self.stack.pop()
                if cond == 0:
                    ip = arg

            elif op == OpCode.OP_RET:
                return self.stack.pop() if self.stack else 0

            else:
                raise RuntimeError(f"Instruction inconnue : {op}")

        return 0