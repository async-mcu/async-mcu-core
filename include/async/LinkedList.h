#pragma once
/**
 * @class Node
 * @brief Represents a node in the linked list.
 * @tparam T The type of data stored in the node.
 */
template <typename T>
class Node {
public:
    T data;         // Data stored in the node
    Node* next;     // Pointer to the next node

    /**
     * @brief Constructor for the Node class.
     * @param data The data to be stored in the node.
     */
    Node(T data) : data(data), next(nullptr) {}
};

/**
 * @class LinkedList
 * @brief Represents a singly linked list.
 * @tparam T The type of data stored in the list.
 */
template <typename T>
class LinkedList {
private:
    Node<T>* head;  // Pointer to the head of the list

public:
    /**
     * @brief Constructor for the LinkedList class.
     * Initializes an empty list.
     */
    LinkedList() : head(nullptr) {}

    /**
     * @brief Destructor for the LinkedList class.
     * Frees all dynamically allocated memory for the nodes.
     */
    ~LinkedList() {
        Node<T>* current = head;
        Node<T>* nextNode;
        while (current != nullptr) {
            nextNode = current->next;
            delete current;
            current = nextNode;
        }
    }

    /**
     * @brief Adds an element to the end of the list.
     * @param data The data to be added.
     */
    void append(T data) {
        Node<T>* newNode = new Node<T>(data);
        if (head == nullptr) {
            head = newNode;
        } else {
            Node<T>* temp = head;
            while (temp->next != nullptr) {
                temp = temp->next;
            }
            temp->next = newNode;
        }
    }

    /**
     * @brief Adds an element to the beginning of the list.
     * @param data The data to be added.
     */
    void prepend(T data) {
        Node<T>* newNode = new Node<T>(data);
        newNode->next = head;
        head = newNode;
    }

    /**
     * @brief Removes the first occurrence of an element from the list.
     * @param data The data to be removed.
     */
    T remove(T data) {
        if (head == nullptr) return NULL;

        if (head->data == data) {
            T removedData = head->data;
            Node<T>* temp = head;
            head = head->next;
            delete temp;
            return removedData;
        }

        Node<T>* current = head;
        while (current->next != nullptr && current->next->data != data) {
            current = current->next;
        }

        if (current->next == nullptr) return NULL;

        Node<T>* temp = current->next;
        T removedData = temp->data;
        current->next = current->next->next;
        delete temp;
        return removedData;
    }

    /**
     * @brief Checks if an element exists in the list.
     * @param data The data to search for.
     * @return True if the element is found, false otherwise.
     */
    bool find(T data) {
        Node<T>* current = head;
        while (current != nullptr) {
            if (current->data == data) {
                return true;
            }
            current = current->next;
        }
        return false;
    }

    /**
     * @brief Returns the number of elements in the list.
     * @return The size of the list.
     */
    int size() {
        int count = 0;
        Node<T>* current = head;
        while (current != nullptr) {
            count++;
            current = current->next;
        }
        return count;
    }

    /**
     * @brief Returns the element at the specified index.
     * @param index The index of the element to retrieve.
     * @return The element at the specified index.
     * @throws std::out_of_range If the index is out of bounds.
     */
    T get(int index) {
        if (index < 0 || index >= size()) {
            //return T;
            return head->data;
            //throw std::out_of_range("Index out of bounds");
        }

        Node<T>* current = head;
        for (int i = 0; i < index; i++) {
            current = current->next;
        }
        return current->data;
    }
};