README
======
This directory contains the Python version of the scraping example.  It was
written against Python 3.7.  It uses the `dataclasses`_ module to simplify
"plain old data" classes as well as the `requests`_ library for making HTTP
requests and `beautifulsoup`_ to parse the HTML response.

Running
-------
Start by creating a new virtual environment, install the requirements, and
run the script.  It should *just work*.

.. code-block:: bash

   $ python3.7 -mvenv env
   $ . ./env/bin/activate
   (env) $ pip install -qr requirements.txt
   (env) $ ./scrape -h
   usage: scrape [-h] [--verbose,-v] URL OUTPUT

   positional arguments:
     URL           the URL to scrape
     OUTPUT        place output in this file

   optional arguments:
     -h, --help    show this help message and exit
     --verbose,-v  enable diagnostic output

   $ ./scrape https://www.177milkstreet.com/recipes/maple-whiskey-pudding-cakes output.html
   I scrape: retrieving page from https://www.177milkstreet.com/recipes/maple-whiskey-pudding-cakes
   I scrape: parsing response data (144049 bytes)
   I scrape: writing output to output.html

.. _beautifulsoup: https://www.crummy.com/software/BeautifulSoup/bs4/doc/
.. _dataclasses: https://docs.python.org/3/library/dataclasses.html
.. _requests: https://requests.readthedocs.io/
