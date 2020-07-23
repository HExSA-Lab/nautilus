#!/usr/bin/python

import os
import sys
import logging
import argparse
import subprocess
import multiprocessing
from shutil import copyfile, rmtree
# pip install pyyaml --user
from yaml import load 

test_matrix   = '.test-matrix.yml'
testdir       = '.tests'
goodtest_code = 99
quiet         = False
DEVNULL       = None

def clean_grub(name):
    rmtree(str(testdir + "/" + name))

def gen_grub(name, flags):
    target  = str(testdir + "/" + name)
    bootdir = str(target + "/boot")
    grubdir = str(bootdir + "/grub")
    grubcfg = str(grubdir + "/grub.cfg")

    os.makedirs(grubdir)

    copyfile("nautilus.bin", str(bootdir + "/nautilus.bin"))
    copyfile("nautilus.syms", str(bootdir + "/nautilus.syms"))
    copyfile("nautilus.secs", str(bootdir + "/nautilus.secs"))

    cmd = ["scripts/gen_grub.py"]
    cmd.extend(flags)

    logging.info("  Generating new grub configuration (%s)", ' '.join(cmd))

    cfg = subprocess.check_output(cmd)

    logging.debug(cfg)

    fd = open(grubcfg, 'w')
    fd.write(cfg)
    fd.close()


def cleandirs():
    rmtree(testdir)


def build_iso(name, flags):
    target = str(testdir + "/" + name)
    cmd    = ["grub-mkrescue", "-o", "nautilus.iso",  target]

    gen_grub(name, flags)

    if quiet:
        subprocess.check_call(cmd, stdout=DEVNULL, stderr=DEVNULL)
    else:
        subprocess.check_call(cmd)

    clean_grub(name)


def build_binary():
    logging.info("  Building binary")
    cpus = multiprocessing.cpu_count()
    cmd  = ["make", "-j" + str(cpus)]

    if quiet:
        ret = subprocess.check_call(cmd, stdout=DEVNULL, stderr=DEVNULL)
    else:
        ret = subprocess.check_call(cmd)


def run(script):
    logging.info("  Running run-script (%s)", script)

    if quiet:
        ret = subprocess.call(script, shell=True, stdout=DEVNULL, stderr=DEVNULL)
    else:
        ret = subprocess.call(script, shell=True)

    if ret is not goodtest_code:
        raise Exception('Test run produced FAILED result')

    logging.info("  Test run returned PASSED result (%d)", ret)


def prep(script):
    logging.info("  Running preparation script (%s)", script)
    if quiet:
        ret = subprocess.check_call(script, stdout=DEVNULL, stderr=DEVNULL, shell=True)
    else:
        ret = subprocess.check_call(script, shell=True)


def run_test_with_cfg(name, prep_str, run_script, flags, cfg):
    logging.info("  * Using config '%s'", name)

    if not os.path.isfile(str("configs/testing/" + cfg + ".config")):
        raise Exception('No such file: {}.config'.format(cfg))

    prep(prep_str)

    copyfile(str("configs/testing/" + cfg + ".config"), ".config")

    logging.info("  Building with config '%s'", cfg)

    build_binary()
    build_iso(name, flags)
    run(run_script)


def run_test(test_dict):

    test_name, test_cfg = test_dict.popitem()

    logging.info("Running test '%s'", test_name)

    for cfg in test_cfg['configs']:
        run_test_with_cfg(test_name, test_cfg['prep'], test_cfg['run'], test_cfg['test_flags'], cfg)


def run_tests(test_matrix):

    fd    = open(test_matrix, 'r')
    tests = load(fd)

    for test in tests:
        run_test(test)

    fd.close()
    cleandirs()


parser = argparse.ArgumentParser(description='Run Nautilus tests.')
parser.add_argument('-v', '--verbose', action='count', help='increase the verbosity of logging')
parser.add_argument('-q', '--quiet', help='suppress output of commands invoked by the test harness', action='store_true')

args = parser.parse_args()

if args.verbose >= 2: 
    logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)
elif args.verbose >= 1:
    logging.basicConfig(stream=sys.stderr, level=logging.INFO)
else:
    logging.basicConfig(stream=sys.stderr, level=logging.WARNING)

if args.quiet:
    quiet = True

try:
    from subprocess import DEVNULL
except ImportError:
    DEVNULL = open(os.devnull, 'wb')

run_tests(test_matrix)
