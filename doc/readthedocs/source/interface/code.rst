============
代码块
============

bash代码样例
========================================================


.. code-block:: bash

        #!/bin/bash
        #================================================
        #FileName   :test_select_46_zhaojiedi.sh
        #Author     :zhaojiedi
        #Description:
        #DateTime   :2017-12-23 10:15:57
        #Version    :V1.0
        #Other      :
        #================================================

        select c in yes no ; do
                echo " you enter is $c"
                case $c in
                        yes)
                                echo "yes";;
                        no)
                                echo "no";;
                        *)
                                echo "other";;
                esac
        done

python代码样例
===========================================

.. code-block:: python


    try:
        f = open('/path/to/file', 'r')
        print(f.read())
    finally:
        if f:
            f.close()

    with open('/path/to/file', 'r') as f:
        print(f.read())

    f = open('/Users/michael/gbk.txt', 'r', encoding='gbk', errors='ignore')

json对象样例展示
====================================================

.. code-block:: json

    {
        "name": "BeJson",
        "url": "http://www.bejson.com",
        "page": 88,
        "isNonProfit": true,
        "address": {
            "street": "科技园路.",
            "city": "江苏苏州",
            "country": "中国"
        },
        "links": [
            {
                "name": "Google",
                "url": "http://www.google.com"
            },
            {
                "name": "Baidu",
                "url": "http://www.baidu.com"
            },
            {
                "name": "SoSo",
                "url": "http://www.SoSo.com"
            }
        ]
    }

异步调用：


.. code-block:: java

	//set，Lets modify the value in our smart contract
	contract.newGreeting("Well hello again3").submit(new Callback<JSONObject>() {
		@Override
		public void called(JSONObject args) {
			System.out.println(args);
		}
	});