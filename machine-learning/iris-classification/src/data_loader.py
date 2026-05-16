"""数据加载与预处理模块"""
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler

# 鸢尾花特征名称
FEATURE_NAMES = ['花萼长度', '花萼宽度', '花瓣长度', '花瓣宽度']
CLASS_NAMES = ['Iris-setosa', 'Iris-versicolor', 'Iris-virginica']
CLASS_NAMES_CN = ['山鸢尾', '变色鸢尾', '弗吉尼亚鸢尾']


def load_iris_data(filepath):
    """从CSV文件加载鸢尾花数据集

    Args:
        filepath: 数据文件路径，每行格式为 "特征1,特征2,特征3,特征4,类别"

    Returns:
        X: 特征矩阵 (n_samples, 4)
        y: 标签数组 (n_samples,)
    """
    X_list = []
    y_list = []

    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(',')
            if len(parts) != 5:
                continue
            features = [float(x) for x in parts[:4]]
            label = parts[4].strip()
            X_list.append(features)
            y_list.append(label)

    # 将类别名称映射为数字
    class_to_idx = {name: i for i, name in enumerate(CLASS_NAMES)}
    y = np.array([class_to_idx.get(label, -1) for label in y_list])
    X = np.array(X_list)

    # 过滤无效标签
    valid_mask = y >= 0
    return X[valid_mask], y[valid_mask]


def preprocess_data(X, test_size=0.3, random_state=42):
    """数据预处理：划分训练集/测试集，并进行标准化

    Args:
        X: 特征矩阵
        test_size: 测试集比例
        random_state: 随机种子

    Returns:
        X_train, X_test, y_train, y_test, scaler
    """
    # 需要同时传入X和y来保证划分一致
    return None  # 实际使用时调用 split_and_scale


def split_and_scale(X, y, test_size=0.3, random_state=42):
    """划分数据集并标准化

    Args:
        X: 特征矩阵
        y: 标签数组
        test_size: 测试集比例
        random_state: 随机种子

    Returns:
        X_train, X_test, y_train, y_test, scaler
    """
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=test_size, random_state=random_state, stratify=y
    )

    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)

    return X_train_scaled, X_test_scaled, y_train, y_test, scaler


def load_and_prepare(filepath, test_size=0.3, random_state=42):
    """一站式加载和预处理

    Args:
        filepath: 数据文件路径
        test_size: 测试集比例
        random_state: 随机种子

    Returns:
        X_train, X_test, y_train, y_test, scaler, X_raw, y_raw
    """
    X, y = load_iris_data(filepath)
    X_train, X_test, y_train, y_test, scaler = split_and_scale(
        X, y, test_size, random_state
    )
    return X_train, X_test, y_train, y_test, scaler, X, y
