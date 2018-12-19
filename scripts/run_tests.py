#!/usr/bin/python

import os
import sys
import logging
import subprocess
from shutil import copyfile, rmtree
# pip install pyyaml --user
from yaml import load 

test_matrix = '.test-matrix.yml'
testdir     = '.tests'
goodtest_code = 99

logging.basicConfig(stream=sys.stderr, level=logging.DEBUG)


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
    gen_grub(name, flags)
    target = str(testdir + "/" + name)
    cmd    = ["grub-mkrescue", "-o", "nautilus.iso",  target]
    subprocess.check_call(cmd)
    clean_grub(name)


def build_binary():
    logging.info(" Building binary")
    subprocess.check_call(["make", "-j6"])


def run(script):
    logging.info("  Running run-script (%s)", script)
    ret = subprocess.call(script, shell=True)

    if ret is not goodtest_code:
        raise Exception('Test run produced FAILED result')

    logging.info("Test run returned PASSED result (%d)", ret)


def prep(script):
    logging.info("  Running preparation script (%s)", script)
    subprocess.check_call(script, shell=True)


def run_test_with_cfg(name, prep_str, run_script, flags, cfg):

    logging.info("  Using config '%s'", name)

    if not os.path.isfile(str("configs/" + cfg + ".config")):
        raise Exception('No such file: {}.config'.format(cfg))

    prep(prep_str)

    copyfile(str("configs/" + cfg + ".config"), ".config")

    logging.info("  Building with config '%s'", cfg)

    build_binary()
    build_iso(name, flags)
    run(run_script)


def run_test(test_dict):

    test_name, test_cfg = test_dict.popitem()

    print "Running test '" + test_name + "'"

    for cfg in test_cfg['configs']:
        run_test_with_cfg(test_name, test_cfg['prep'], test_cfg['run'], test_cfg['test_flags'], cfg)


def run_tests(test_matrix):

    fd = open(test_matrix, 'r')
    tests = load(fd)

    for test in tests:
        run_test(test)

    fd.close()

    cleandirs()


run_tests(test_matrix)
