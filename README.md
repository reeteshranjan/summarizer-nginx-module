summarizer-nginx-module
=======================

Upstream nginx module to get summaries of documents using the summarizer daemon
service.

Status

    Alpha [Work in progress]

Version

    0.1

Synopsis

    # Summary of local files
    # /summary?filename=xyz&ratio=30.00
    location /summary {
        set_unescape_uri    $smrzr_filename    $arg_filename;
        set_unescape_uri    $smrzr_ratio       $arg_ratio;
        sphinx2_pass        127.0.0.1:9872;
    }

Description

    This is an Nginx upstream module that makes nginx talk to the summarizer
    service (https://github.com/reeteshranjan/summarizer). The summarizer is a
    highly optimized version of the open source text summarizer known as OTS
    (http://libots.sourceforge.net/), now available at github (
    https://github.com/neopunisher/Open-Text-Summarizer).

    The summarizer service uses non-blocking I/O as well to support overall CPU
    usage optimization and higher degree of performance.

    The summarizer service is designed to provide summarization of "local" files
    as of now. This version of the module supports this mode of summarization.

    The module outputs the summary utf-8 text (all headers etc. are consumed by
    the module).

Compatibility

    Verified with:
    
    *   nginx-1.4.3
    *   summarizer-1.0

Limitations

    *   Portions of code are 64-bit specific.

Installation Instructions

    *   Get nginx source from nginx.org (see "Comptability" above)

    *   Download a release tarball of summarizer-nginx-module from
        https://github.com/reeteshranjan/summarizer-nginx-module/releases

    *   Include --add-module=/path/to/summarizer-nginx-module directive in the
        configure command for building nginx source.

    *   make & make install (assuming you have permission for install folders)

    *   Use sample nginx conf file section above to modify your nginx conf.
