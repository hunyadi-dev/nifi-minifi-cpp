import sys
sys.path.append('../minifi')
import logging
import datetime

from MiNiFi_integration_test_driver import MiNiFi_integration_test
from minifi import *

def raise_exception(exception):
  raise exception

def integration_test_cleanup(test):
  logging.info("Integration test cleanup...")
  del test

def before_scenario(context, scenario):
  logging.info("Integration test setup at {time:%H:%M:%S:%f}".format(time=datetime.datetime.now()))
  context.test = MiNiFi_integration_test(context)

def after_scenario(context, scenario):
  logging.info("Integration test teardown at {time:%H:%M:%S:%f}".format(time=datetime.datetime.now()))
  if context is not None and hasattr(context, "test"):
    context.test.cleanup() # force invocation
    del context.test
  else:
    raise Exception("Unable to manually clean up test context. Might already be deleted?")

def before_all(context):
    context.config.setup_logging()
