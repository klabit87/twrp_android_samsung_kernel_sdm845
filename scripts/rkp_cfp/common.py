import re
import pprint

hex_re = r'(?:[a-f0-9]{16})'
hex_rec = re.compile(hex_re)

reg_re = r'\b(?:(?:x|w)(\d+))\b'
reg_rec = re.compile(reg_re)

fun_re = r'(?P<func_addr>' + hex_re + ') <(?P<func_name>[^>]+)>:$'
fun_rec = re.compile(fun_re)

ident_re = r'(?:[a-zA-Z_][a-zA-Z0-9_]*)'
ident_rec = re.compile(ident_re)

class MyPrettyPrinter(pprint.PrettyPrinter):
    def format(self, object, context, maxlevels, level):
        if isinstance(object, unicode):
            return (object.encode('utf8'), True, False)
        return pprint.PrettyPrinter.format(self, object, context, maxlevels, level)

_printer = MyPrettyPrinter()
def pr(x):
    return _printer.pprint(x)

def run_from_ipython():
    try:
        __IPYTHON__
        return True
    except NameError:
        return False

class Log(object):
    def __init__(self, filename=None):
        self.filename = filename
        self.f = None
        if self.filename is not None:
            self.f = open(filename, 'w+')

    def __call__(self, msg=''):
        if self.f is not None:
            self.f.write(msg)
            self.f.write('\n')
            self.f.flush()

    def __enter__(self):
        pass

    def __exit__(self, type, value, traceback):
        if self.f is not None:
            self.f.close()
            self.f = None
# Default logging object just print to stdout
LOG = Log()

def log(msg=''):
    global Log
    LOG(msg)


"""
Centralized skip and CONFIG flag
"""
#File containing functions to skip instrumenting
skip = set([])

"""
File containing assembly functions that have been manually inspected to
disable preemption/interrupts instead of doing 'stp x29, x30'
(i.e. don't error out during validation for these functions)
"""
skip_save_lr_to_stack = set([
    'flush_cache_all',
    'flush_cache_louis'])



skip_stp = set([
    '__cpu_suspend_enter',
    'save_stack',
])

#File containing assembly file paths whose functions we should skip instrumenting
skip_asm = set([])


#ASM functions code that are permitted to have br instructions in them
skip_br=set([
    'stext',
    '__turn_mmu_on',
    'el0_svc_naked',
    '__sys_trace',
    'fpsimd_save_partial_state',
    'fpsimd_load_partial_state',
    'cpu_resume_mmu' ])

skip_blr=set([
    'stext',
    '__primary_switch',
    '__primary_switched',
    'start_kernel',
    'secondary_startup',
    'el0_svc_naked',
    '__sys_trace',
    'fimc_is_lib_vra_os_funcs',
    'fimc_is_hw_vra_init',
    'fimc_is_hw_3aa_init',
    'fimc_is_hw_dcp_init',
    'fimc_is_hw_tpu_init',
    'fimc_is_hw_isp_init',
    'fimc_is_load_ddk_bin',
    'fimc_is_load_rta_bin',
    'sensor_module_init',
    'fimc_is_itf_sensor_mode_wrap',
    ])

skip_magic=set([
    'ropp_enable_backtrace',
    'rkp_call',

    # do_execve
    'try_to_run_init_process',
    'run_init_process',

    #cred related
    'copy_creds',
    'commit_creds',
    'exit_creds',
    'get_new_cred',
    'get_task_cred',
    'prepare_creds',
    'prepare_kernel_cred',
    'put_cred',
    'put_ro_cred',
    'rkp_free_security',
    'rkp_get_init_cred',
    'rkp_get_usecount',
    'rkp_override_creds',
    'revert_creds',
    'set_security_override',
    'set_security_override_from_ctx',
    ])
