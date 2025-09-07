import sys
sys.path.append("objdumpeg")
from objdumpeg.CodeBlock import CodeBlock
from objdumpeg.Binary import Binary
import os
import re
import logging
logger = logging.getLogger(__name__)

class InstrTemplate:
    def __init__(self, filepath):
        self.filepath = filepath
        self.content = getcontent(filepath)
        self.tempregs = []
        self.oprand = []
        self.liveregs = []
        self.tempFREG_num = 0
        self.tempREG_num = 0


    def read_info_from_str(self):
        oppattern = r'^;\s*operand:'
        livepattern  = r'^;\s*live REGS:'
        temporarypattern  = r'^;\s*Temporary REGS:'
        for s in self.content:
            if re.match(oppattern, s):
                self.oprand = splitregs(s)
            if re.match(livepattern, s):
                self.liveregs = splitregs(s)
            if re.match(temporarypattern, s):
                self.tempregs = splitregs(s)
    
    # Incorporate the register information from the original extension instruction into content, treating the extension register as an address
    def update_oprand(self, regs): #regs is a list
        regdict = {}
        if len(regs) != len(self.oprand):
            raise Exception("input regs info error!")
        for i, x in enumerate(self.oprand):
            regdict[x] = regs[i]
        self.opranddict = regdict
        for REG in regdict.keys():
            reg = regdict[REG]
            #replace the REG in s (a line of content) with reg
            self.content = [replace_REG(s, REG, reg) for s in self.content]
        logger.debug("update oprands:{}".format(self.content))
            

    def update_tempregs(self, regs): #regs is a list
        regdict = {}
        #print(len(self.tempregs))
        if len(regs) != len(self.tempregs):
            raise Exception("input regs info error!")
        for i, x in enumerate(self.tempregs):
            regdict[x] = regs[i]
        self.tempregsdict = regdict
        for REG in regdict.keys():
            reg = regdict[REG]
            #replace the REG in s (a line of content) with reg
            self.content = [replace_REG(s, REG, reg) for s in self.content]
        logger.debug("update temp regs:{}".format(self.content))

    def return_liveregs(self):
        liveres = []
        for REG in self.liveregs:
            if REG in self.opranddict:
                liveres.append(self.opranddict[REG])
        return liveres

    def get_tmpFREG_num(self):
        self.tepFREG_num = 0
        for x in self.tempregs:
            if x[0] == 'F':
                self.tempFREG_num += 1
            else:
                self.tempREG_num += 1


    def return_content(self):
        return "\n".join([x for x in self.content if x[0] != ";"])

    def __repr__(self):
        return (f"oprand='{self.oprand}', liveregs='{self.liveregs}', tempregs='{self.tempregs}'")

def extract_parenthesized_content(input_string):
    # 正则表达式模式，用于匹配括号中的内容
    pattern = r'\(([^()]+)\)'
    
    matches = re.findall(pattern, input_string)
    if len(matches) == 0:
        return False, matches

    return True, matches

#s是需要匹配的字符，REG是模版中的寄存器，reg是实际分配的寄存器
def REG_cmp(s, REG, reg):
    pattern = r'^-?\d+\s*'
    if s.lstrip().strip() == REG:
        return reg
    elif s.lstrip().strip() == REG+',': #REG,
        return reg + ","
    else: #num(REG)
        findmacth, matches = extract_parenthesized_content(s)
        if findmacth and len(matches) == 1 and REG == matches[0]:
            return s.replace(matches[0], reg)
    return s


def replace_REG(s, REG, reg):
    if not s:
        return s
    if s[0] == ';':
        return s
    spart = s.split()
    return " ".join([REG_cmp(x, REG, reg) for x in spart])


def splitregs(s):
    regs = s.split(":")[1]
    return [x.lstrip().strip() for x in regs.split(',') if x != '']

def getcontent(filename):
    content = []
    with open(filename, "r") as f:
        for l in f.readlines():
            content.append(l.strip())
    #print(content)
    return content

if __name__ == "__main__":
    #t = InstrTemplate("tranblocks/inst/vfadd.vv.s")
    t = InstrTemplate("tranblocks/inst/add.uw.s")
    t.read_info_from_str()
    print(t.return_content())
    t.update_oprand(["a1", "a2", "a3"])
    t.update_tempregs([])
    print(t.return_content())




