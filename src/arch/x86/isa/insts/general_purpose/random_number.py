microcode = """
def macroop RDRAND_R
{
    rdrandop reg, t0, t0, flags=(CF, OF, SF, ZF, AF, PF)
};

def macroop RDRAND_M
{
    fault "std::make_shared<InvalidOpcode>()"
};

def macroop RDRAND_P
{
    fault "std::make_shared<InvalidOpcode>()"
};

def macroop RDSEED_R
{
    rdseedop reg, reg, t0, flags=(CF, OF, SF, ZF, AF, PF)
};

def macroop RDSEED_M
{
    fault "std::make_shared<InvalidOpcode>()"
};

def macroop RDSEED_P
{
    fault "std::make_shared<InvalidOpcode>()"
};
"""
