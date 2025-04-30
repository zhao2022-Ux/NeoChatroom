import requests
from concurrent.futures import ThreadPoolExecutor, as_completed
import time

# 配置
URL = "http://127.0.0.1:80"
TOTAL_REQUESTS = 1000  # 总请求数
CONCURRENT_THREADS = 50  # 并发线程数

def send_request():
    """发送单个HTTP GET请求"""
    try:
        response = requests.get(URL)
        return response.status_code
    except requests.exceptions.RequestException as e:
        return f"Error: {e}"

def stress_test():
    """执行压力测试"""
    start_time = time.time()
    success_count = 0
    error_count = 0

    with ThreadPoolExecutor(max_workers=CONCURRENT_THREADS) as executor:
        futures = [executor.submit(send_request) for _ in range(TOTAL_REQUESTS)]
        for future in as_completed(futures):
            result = future.result()
            if result == 200:
                success_count += 1
            else:
                error_count += 1

    end_time = time.time()
    print(f"测试完成！")
    print(f"总请求数: {TOTAL_REQUESTS}")
    print(f"成功请求数: {success_count}")
    print(f"失败请求数: {error_count}")
    print(f"总耗时: {end_time - start_time:.2f} 秒")
    print(f"每秒请求数: {TOTAL_REQUESTS / (end_time - start_time):.2f}")

if __name__ == "__main__":
    stress_test()
