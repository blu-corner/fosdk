#!/usr/bin/python
from jinja2 import Environment, FileSystemLoader
from xhtml2pdf import pisa
import optparse
import logging
import json
import sys

logging.basicConfig(format='%(asctime)s - %(levelname)s - %(message)s', level=logging.INFO)

env = Environment(loader=FileSystemLoader('.'))


class Reporter:

    def __init__(self, results, template):
        self._results = results
        self._template = env.get_template(template)

    @property
    def logger(self):
        logger = logging.getLogger(__name__)
        return logger

    @property
    def results(self):
        return self._results

    @property
    def template(self):
        return self._template

    def template_results(self):
        with open("{}/order-acked-consolidated-results.json".format(self.results)) as ack_js:
            ack = json.load(ack_js)
        with open("{}/order-entry-consolidated-results.json".format(self.results)) as entry_js:
            ent = json.load(entry_js)
        with open("{}/round-trip-consolidated-results.json".format(self.results)) as rt_js:
            rt = json.load(rt_js)
        with open("{}/systemInfo.json".format(self.results)) as info_js:
            sys = json.load(info_js)

        template_vars = {"ack": ack, "ent": ent, "source": self.results, "sys": sys, "rt": rt}

        temp_out = self.template.render(template_vars)

        with open("{}/report.html".format(self.results), 'wb') as oa:
            oa.write(temp_out)
            self.logger.info("report.html created")
        with open("{}/report.pdf".format(self.results), 'wb') as pf:
            pisa.CreatePDF(src=temp_out, dest=pf)
            self.logger.info("report.pdf created")


if __name__ == "__main__":
    parser = optparse.OptionParser()
    parser.add_option("--results", dest="results", help="Path to results directory")
    parser.add_option("--template", dest="template", help="Path to template file")
    options, inputs = parser.parse_args()

    template = options.template
    if template is None:
        sys.exit("Please give directory to your template")
    results = options.results
    if results is None:
        sys.exit("Please give directory to your analysis results")

    report = Reporter(results, template)
    report.template_results()
