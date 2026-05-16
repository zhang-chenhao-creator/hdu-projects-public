"""分类算法模块 - 策略模式实现"""
from abc import ABC, abstractmethod
from sklearn.neighbors import KNeighborsClassifier
from sklearn.tree import DecisionTreeClassifier
from sklearn.svm import SVC
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix
import numpy as np


class Classifier(ABC):
    """分类器抽象基类（策略接口）"""

    def __init__(self, name):
        self.name = name
        self.model = None
        self.is_trained = False

    @abstractmethod
    def _create_model(self):
        """创建模型实例"""
        pass

    def train(self, X_train, y_train):
        """训练模型"""
        self.model = self._create_model()
        self.model.fit(X_train, y_train)
        self.is_trained = True

    def predict(self, X):
        """预测"""
        if not self.is_trained:
            raise RuntimeError("模型尚未训练，请先调用 train()")
        return self.model.predict(X)

    def evaluate(self, X_test, y_test):
        """评估模型

        Returns:
            dict: 包含 accuracy, report, confusion_matrix, predictions
        """
        y_pred = self.predict(X_test)
        return {
            'accuracy': accuracy_score(y_test, y_pred),
            'report': classification_report(y_test, y_pred, output_dict=True),
            'confusion_matrix': confusion_matrix(y_test, y_pred),
            'predictions': y_pred,
        }


class KNNClassifier(Classifier):
    """K近邻分类器"""

    def __init__(self, n_neighbors=5):
        super().__init__(f"KNN (k={n_neighbors})")
        self.n_neighbors = n_neighbors

    def _create_model(self):
        return KNeighborsClassifier(n_neighbors=self.n_neighbors)


class DecisionTreeClassifierWrapper(Classifier):
    """决策树分类器"""

    def __init__(self, max_depth=None, random_state=42):
        depth_str = max_depth if max_depth else "不限"
        super().__init__(f"决策树 (深度={depth_str})")
        self.max_depth = max_depth
        self.random_state = random_state

    def _create_model(self):
        return DecisionTreeClassifier(
            max_depth=self.max_depth,
            random_state=self.random_state
        )


class SVMClassifier(Classifier):
    """支持向量机分类器"""

    def __init__(self, kernel='rbf', C=1.0, random_state=42):
        super().__init__(f"SVM (核函数={kernel})")
        self.kernel = kernel
        self.C = C
        self.random_state = random_state

    def _create_model(self):
        return SVC(
            kernel=self.kernel,
            C=self.C,
            random_state=self.random_state,
            probability=True
        )


# 分类器工厂
CLASSIFIER_MAP = {
    'knn': lambda: KNNClassifier(),
    'decision_tree': lambda: DecisionTreeClassifierWrapper(),
    'svm': lambda: SVMClassifier(),
}

CLASSIFIER_NAMES = {
    'knn': 'K近邻 (KNN)',
    'decision_tree': '决策树',
    'svm': '支持向量机 (SVM)',
}


def create_classifier(algorithm):
    """工厂方法：根据名称创建分类器

    Args:
        algorithm: 算法标识 ('knn', 'decision_tree', 'svm')

    Returns:
        Classifier 实例
    """
    if algorithm not in CLASSIFIER_MAP:
        raise ValueError(f"不支持的算法: {algorithm}，可选: {list(CLASSIFIER_MAP.keys())}")
    return CLASSIFIER_MAP[algorithm]()


def get_all_classifiers():
    """获取所有可用分类器"""
    return {name: factory() for name, factory in CLASSIFIER_MAP.items()}
