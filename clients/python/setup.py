from setuptools import setup, find_packages

with open('README.md', 'r', encoding='utf-8') as fd:
    long_description = fd.read()

setup(
    name='disqueue',
    version='0.0.1',
    author='fkfv',
    author_email='20618646+fkfv@users.noreply.github.com',
    description='Disqueue Python client',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/fkfv/disqueue',
    project_urls={
      'Bug Tracker': 'https://github.com/fkfv/disqueue/issues'
    },
    classifiers=[
      'Programming Language :: Python :: 3',
      'License :: OSI Approved :: MIT License',
      'Operating System :: OS Independent'
    ],
    package_dir={'': 'src'},
    packages=find_packages(where='src'),
    python_requires='>=3.4'
)
